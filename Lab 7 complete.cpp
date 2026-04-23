//=====[Libraries]=============================================================

#include "smart_home_system.h"

/* ================================================================
   Main Task 7 - Smart Home Motor Control with Interrupts
   Platform: NUCLEO-F439ZI, Mbed OS 6, Keil Studio Cloud

   Requirements met:
   - DC motor driven via two relays (H-bridge) + PWM speed control
   - Hardware interrupt on D2 triggers motor state change
   - I2C LCD shows interrupt count, motor state, direction, speed
   - 4x4 keypad controls direction, speed, and operating mode
   - Serial monitor reports every event with timestamp
   ================================================================ */

#include "mbed.h"
#include <cstdio>
#include <cstring>
using namespace std::chrono;

// ================= I2C LCD DRIVER =================
class I2CLcd {
private:
    I2C &i2c;
    int address;
    char backlight;

    void writeExpander(char data) {
        char buf[1];
        buf[0] = data | backlight;
        i2c.write(address, buf, 1);
    }
    void pulse(char data) {
        writeExpander(data | 0x04); wait_us(1);
        writeExpander(data & ~0x04); wait_us(50);
    }
    void write4bits(char val) { writeExpander(val); pulse(val); }
    void send(char val, char mode) {
        write4bits((val & 0xF0) | mode);
        write4bits(((val << 4) & 0xF0) | mode);
    }

public:
    I2CLcd(I2C &bus, int addr) : i2c(bus), address(addr), backlight(0x08) {}

    void init() {
        ThisThread::sleep_for(50ms);
        writeExpander(backlight);
        write4bits(0x30); ThisThread::sleep_for(5ms);
        write4bits(0x30); wait_us(150);
        write4bits(0x30);
        write4bits(0x20);
        command(0x28); command(0x0C); command(0x06);
        clear();
    }
    void command(char v) { send(v, 0); }
    void putc(char v) { send(v, 1); }
    void print(const char* s) { while (*s) putc(*s++); }
    void clear() { command(0x01); ThisThread::sleep_for(2ms); }
    void locate(int c, int r) {
        int offsets[] = {0x00, 0x40, 0x14, 0x54};
        command(0x80 | (c + offsets[r]));
    }
    void printLine(int r, const char* txt) {
        char buf[21];
        snprintf(buf, 21, "%-20s", txt);
        locate(0, r); print(buf);
    }
};

// ================= HARDWARE =================
I2C         i2c(PB_9, PB_8);
I2CLcd      lcd(i2c, 0x4E);

DigitalOut  relayFwd(D7);
DigitalOut  relayRev(D8);
PwmOut      motorPwm(D9);

InterruptIn userBtn(D2, PullUp);     // falling-edge trigger
DigitalOut  actLed(LED1);

DigitalOut  rowPins[4] = {PB_3, PB_5, PC_7, PA_15};
DigitalIn   colPins[4] = {PB_12, PB_13, PB_15, PC_6};

const char keyMap[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

// ================= STATE =================
enum MotorState { MOTOR_OFF, MOTOR_FORWARD, MOTOR_REVERSE };
enum Mode       { MODE_MANUAL, MODE_AUTO };

MotorState    motor_state = MOTOR_OFF;
Mode          op_mode     = MODE_MANUAL;
float         motor_speed = 0.5f;              // 0.0 to 1.0
volatile int  interrupt_count = 0;
volatile bool interrupt_flag  = false;
Timer         sys_timer;
Ticker        auto_ticker;
volatile bool auto_flag = false;

// ================= MOTOR CONTROL =================
void set_motor(MotorState s) {
    // Mutual exclusion: never both relays on at once
    relayFwd = 0;
    relayRev = 0;
    wait_us(500);                              // relay settle time

    switch (s) {
        case MOTOR_OFF:
            motorPwm.write(0.0f);
            break;
        case MOTOR_FORWARD:
            relayFwd = 1;
            motorPwm.write(motor_speed);
            break;
        case MOTOR_REVERSE:
            relayRev = 1;
            motorPwm.write(motor_speed);
            break;
    }
    motor_state = s;
}

const char* state_name(MotorState s) {
    switch (s) {
        case MOTOR_OFF:     return "OFF";
        case MOTOR_FORWARD: return "FORWARD";
        case MOTOR_REVERSE: return "REVERSE";
    }
    return "?";
}

// ================= ISRs =================
void on_user_button() {
    interrupt_count++;
    interrupt_flag = true;
    actLed = !actLed;                          // instant visual feedback
}

void on_auto_tick() { auto_flag = true; }

// ================= KEYPAD =================
char scan_keypad() {
    for (int r = 0; r < 4; r++) {
        for (int i = 0; i < 4; i++) rowPins[i] = 1;
        rowPins[r] = 0;
        wait_us(500);
        for (int c = 0; c < 4; c++) {
            if (colPins[c] == 0) return keyMap[r][c];
        }
    }
    return 0;
}

char get_key() {
    char k = scan_keypad();
    if (k) {
        ThisThread::sleep_for(40ms);
        if (scan_keypad() == k) {
            while (scan_keypad() == k) ThisThread::sleep_for(10ms);
            return k;
        }
    }
    return 0;
}

// ================= LCD =================
void update_lcd() {
    char l0[21], l1[21], l2[21], l3[21];
    snprintf(l0, 21, "Motor: %s", state_name(motor_state));
    snprintf(l1, 21, "Speed: %3d%%",  (int)(motor_speed * 100));
    snprintf(l2, 21, "Mode : %s",  op_mode == MODE_MANUAL ? "MANUAL" : "AUTO");
    snprintf(l3, 21, "INTs : %d", interrupt_count);
    lcd.printLine(0, l0);
    lcd.printLine(1, l1);
    lcd.printLine(2, l2);
    lcd.printLine(3, l3);
}

// ================= SERIAL LOG =================
void log_event(const char* msg) {
    int t = duration_cast<seconds>(sys_timer.elapsed_time()).count();
    printf("[%3ds] %s | State=%s Speed=%d%% Mode=%s INTs=%d\n",
           t, msg, state_name(motor_state),
           (int)(motor_speed * 100),
           op_mode == MODE_MANUAL ? "MANUAL" : "AUTO",
           interrupt_count);
}

// ================= KEYPAD HANDLER =================
void handle_key(char k) {
    switch (k) {
        case '1':
            set_motor(MOTOR_FORWARD);
            log_event("KEY 1: Motor FORWARD");
            break;
        case '2':
            set_motor(MOTOR_REVERSE);
            log_event("KEY 2: Motor REVERSE");
            break;
        case '3':
            set_motor(MOTOR_OFF);
            log_event("KEY 3: Motor STOP");
            break;
        case '4':
            if (motor_speed < 1.0f) motor_speed += 0.1f;
            if (motor_speed > 1.0f) motor_speed = 1.0f;
            if (motor_state != MOTOR_OFF) motorPwm.write(motor_speed);
            log_event("KEY 4: Speed UP");
            break;
        case '5':
            if (motor_speed > 0.1f) motor_speed -= 0.1f;
            if (motor_speed < 0.1f) motor_speed = 0.1f;
            if (motor_state != MOTOR_OFF) motorPwm.write(motor_speed);
            log_event("KEY 5: Speed DOWN");
            break;
        case 'A':
            op_mode = MODE_AUTO;
            auto_ticker.attach(&on_auto_tick, 3s);
            log_event("KEY A: AUTO mode ON");
            break;
        case 'B':
            op_mode = MODE_MANUAL;
            auto_ticker.detach();
            set_motor(MOTOR_OFF);
            log_event("KEY B: MANUAL mode ON");
            break;
        default: return;
    }
    update_lcd();
}

// ================= AUTO CYCLE =================
void auto_advance() {
    switch (motor_state) {
        case MOTOR_OFF:     set_motor(MOTOR_FORWARD); break;
        case MOTOR_FORWARD: set_motor(MOTOR_REVERSE); break;
        case MOTOR_REVERSE: set_motor(MOTOR_OFF);     break;
    }
    log_event("AUTO: advance");
    update_lcd();
}

// ================= MAIN =================
int main() {
    ThisThread::sleep_for(500ms);
    printf("\n=== Task 7: Smart Home Motor Control ===\n");
    printf("Keys: 1=FWD 2=REV 3=STOP 4=SPD+ 5=SPD- A=AUTO B=MANUAL\n");
    printf("INT on D2 button toggles motor instantly.\n\n");

    for (int i = 0; i < 4; i++) colPins[i].mode(PullUp);

    i2c.frequency(100000);
    lcd.init();
    lcd.printLine(0, "Task 7: Motor Ctrl");
    lcd.printLine(1, "Init...");
    ThisThread::sleep_for(1s);

    motorPwm.period_ms(2);
    set_motor(MOTOR_OFF);

    userBtn.fall(&on_user_button);       // attach ISR
    sys_timer.start();
    update_lcd();

    while (true) {
        // --- Interrupt-flag handling (non-ISR context) ---
        if (interrupt_flag) {
            interrupt_flag = false;
            // Interrupt toggles through OFF -> FORWARD -> REVERSE -> OFF
            switch (motor_state) {
                case MOTOR_OFF:     set_motor(MOTOR_FORWARD); break;
                case MOTOR_FORWARD: set_motor(MOTOR_REVERSE); break;
                case MOTOR_REVERSE: set_motor(MOTOR_OFF);     break;
            }
            log_event("INTERRUPT: D2 button");
            update_lcd();
        }

        // --- Auto-mode tick ---
        if (auto_flag) {
            auto_flag = false;
            if (op_mode == MODE_AUTO) auto_advance();
        }

        // --- Keypad ---
        char k = get_key();
        if (k) handle_key(k);

        ThisThread::sleep_for(20ms);
    }
}