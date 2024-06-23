//!  Часи з будильником і режимами
//!  Version 0.1

#include <buildTime.h>         // Для запису часу компіляції
#include <Stamp.h>             // Для взаємодії з часом
#include <EEManager.h>         // Для запису в пам'ять
#include <GyverOS.h>           // Для багатозадачності
#include <GyverTM1637.h>       // Для дисплея часів TM1637
#include <TimerMs.h>           // Для таймерів задач
#include <EncButton.h>         // Для енкодера
#include <NecDecoder.h>        // Для IR пульта
#include "RemoteButtonCodes.h" // Усі коди для IR пульта

#define MEMORY_TIMEOUT 4000 // Чи використовувати SERIAL
#define MIN_BRIGHTNESS 0    // Мінімальна яскравість
#define MAX_BRIGHTNESS 7    // Максимальна яскравість
#define USE_SERIAL true     // Чи використовувати SERIAL
#define USE_ENC false       // Чи використовувати енкодер
#define KEY 7               // ПІН Енкодер KEY
#define A 6                 // ПІН Енкодер A
#define B 5                 // ПІН Енкодер B
#define BUZZER 9            // ПІН Звуковий
#define DIO 4               // ПІН Дисплей DIO
#define CLK 3               // ПІН Дисплей CLK

enum ChangeNumMode : byte // Візуальні режими перемикання часу
{
    MODE_NORMAL = 0, // Звичайно (перемикання цифр)
    MODE_FALL = 1,   // Падіння (перемикання цифр)
    MODE_TWIST = 2   // Скрутка (перемикання цифр)
};

enum ShowClockMode : byte // Режими відображення часу
{
    HOURS_MINUTES = 1,   // Режим відображення часу: ГОД:ХВ
    MINUTES_SECONDS = 2, // Режим відображення часу: ХВ:СЕК
    MONTH_YEAR = 3       // Режим відображення часу: МІСЯЦЬ:РІК
};

enum TimeMode : byte // Режими зміни часу
{
    HOURS = 0,   // Режим зміни часу: ГОДИНИ
    MINUTES = 1, // Режим зміни часу: ХВИЛИНИ
    SECONDS = 2  // Режим зміни часу: СЕКУНДИ
};

struct Settings // Структура для зберігання даних
{
    bool sound_tick = false;                     // Чи є звук секунд
    bool led_tick = false;                       // Чи "тікає" світлодіод
    byte brightness = 1;                         // Яскравість
    ChangeNumMode changeNumMode = MODE_TWIST;    // Режим перемикання цифр
    ShowClockMode showClockMode = HOURS_MINUTES; // Режим показу часу
    TimeMode changeTimeMode = MINUTES;           // Режим зміни часу
    byte alarmH = 6;                             // Година будильника
    byte alarmM = 30;                            // Хвилина будильника
    byte alarmS = 0;                             // Секунда будильника
};

Settings data;                          // Змінна з налаштуваннями
EEManager memory(data, MEMORY_TIMEOUT); // Передаєм данні що будуть в пам'яті
EncButton<EB_TICK, A, B, KEY> enc;      // Eнкодер з кнопкою
GyverTM1637 disp(CLK, DIO);             // 7-сегмент часи (4 цифри)
GyverOS<2> OS;                          // К-сть задач
NecDecoder ir;                          // IR пульт (D2 pin)
TimerMs tmrIR(200, 1, 0);               // Таймер таймаут пульта

int8_t year = BUILD_YEAR;
int8_t month = BUILD_MONTH;
int8_t day = BUILD_DAY;
int8_t hours = BUILD_HOUR;
int8_t minutes = BUILD_MIN;
int8_t seconds = BUILD_SEC;

bool inAlarmMenu;                                 // Чи в меню будильника
ShowClockMode showAlarmClockMode = HOURS_MINUTES; // Режим показу часу в меню будильника

void setup()
{
    DDRB |= (1 << 5); // pinMode(LED_BUILTIN, OUTPUT);
    memory.begin(0, 'g');
    memory.setTimeout(5000);
#if USE_SERIAL
    Serial.begin(9600);
    PrintSerialMemoryData();
#endif
    attachInterrupt(0, irIsr, FALLING);
    OS.attach(0, timeTick, 1000);
    OS.attach(1, displayUpdate, 1001);
    disp.brightness(data.brightness);
}

void loop()
{
    OS.tick();
    memory.tick();
    checkIR();
#if USE_ENC
    checkEnc();
#endif
}

// Оновлення даних в пам'яті
void updateMemoryNow()
{
    memory.updateNow();
}

// Головний таймер секунд
void timeTick()
{
    seconds++;

    if (data.sound_tick)
        tone(BUZZER, 1400, 2);

    checkPlusTime();
    checkAlarmClock(data.alarmH, data.alarmM);
}

// Вивід змінних Data в Serial
void PrintSerialMemoryData()
{
    struct SerialStruct
    {
        SerialStruct(const char *name, byte value, const char *end = "") : name(name), value(value), end(end) {}
        const char *name;
        byte value;
        const char *end;
    };

    SerialStruct variables[] = {
        {"Яскравість: ", data.brightness},
        {"Звук тіка: ", data.sound_tick},
        {"clockMode: ", data.showClockMode},
        {"Режим зміни цифр: ", data.changeNumMode},
        {"Режим зміни розрядності часу: ", data.changeTimeMode},
        {"Будильник: ", data.alarmH, " година"},
        {"Будильник: ", data.alarmM, " хвилина"},
        {"Будильник: ", data.alarmS, " секунда"},
        {"Рік: ", year},
        {"Місяць: ", month},
        {"День: ", day},
        {"Години: ", hours},
        {"Хвилини: ", minutes},
        {"Секунди: ", seconds}};

    for (const SerialStruct &var : variables)
    {
        Serial.print(var.name);
        Serial.print(var.value);
        Serial.println(var.end);
    }
}

void plusTime(bool hour = false, bool minute = false, bool second = false)
{
    if (hour)
    {
        hours++;
    }
    else if (minute)
    {
        minutes++;
    }
    else if (second)
    {
        seconds++;
    }
    checkPlusTime();
    displayClockWithMode();
}

void minusTime(bool hour = false, bool minute = false, bool second = false)
{
    if (hour)
    {
        hours--;
    }
    else if (minute)
    {
        minutes--;
    }
    else if (second)
    {
        seconds--;
    }
    checkMinusTime();
    displayClockWithMode();
}

//! ##############################################  ПУЛЬТ  ##############################################
void checkIR()
{
    if (tmrIR.tick() and ir.available())
    {
        byte code = ir.readInvCommand();
        tone(BUZZER, 1300, 70);

        switch (code)
        {
        case _CH_M:
            if (inAlarmMenu)
            {
                showAlarmClockMode = HOURS_MINUTES;
                disp.displayClock(data.alarmH, data.alarmM);
            }
            else
            {
                if (data.showClockMode != HOURS_MINUTES)
                {
                    data.showClockMode = HOURS_MINUTES;
                    disp.displayClockScroll(hours, minutes, 50);
                }
                else
                    noTone(BUZZER);
            }
            break;

        case _CH:
            data.sound_tick = !data.sound_tick;
            memory.updateNow();
            break;

        case _CH_P:
            if (inAlarmMenu)
            {
                showAlarmClockMode = MINUTES_SECONDS;
                disp.displayClock(data.alarmM, data.alarmS);
            }
            else
            {
                if (data.showClockMode != MINUTES_SECONDS)
                {
                    data.showClockMode = MINUTES_SECONDS;
                    disp.displayClockScroll(minutes, seconds, 50);
                }
                else
                    noTone(BUZZER);
            }
            break;

        case _BACK:
            // Якщо в меню будильника
            if (inAlarmMenu)
            {
                if (data.changeTimeMode == 0 && data.alarmH != 0)
                    data.alarmH--;
                else if (data.alarmM != 0)
                    data.alarmM -= 5;

                disp.displayClock(data.alarmH, data.alarmM);
                memory.update();
            }
            else
            {
                minusTimeWithMode();
            }
            break;

        case _NEXT:
            // Якщо в меню будильника
            if (inAlarmMenu)
            {
                if (data.changeTimeMode == 0 && data.alarmH != 23)
                    data.alarmH++;
                else if (data.alarmM != 55)
                    data.alarmM += 5;

                disp.displayClock(data.alarmH, data.alarmM);
                memory.update();
            }
            else
            {
                plusTimeWithMode();
            }
            break;

        case _PLAY_PAUSE:
            noTone(BUZZER);
            break;

        case _MINUS:
            brightnessMinus();
            break;

        case _PLUS:
            brightnessPlus();
            break;

        case _EQ:
            inAlarmMenu = !inAlarmMenu;
            showAlarmClockMode = HOURS_MINUTES;

            if (inAlarmMenu)
            {
                disp.displayClock(data.alarmH, data.alarmM);
            }
            else
            {
                switch (data.showClockMode)
                {
                case HOURS_MINUTES:
                    disp.displayClock(hours, minutes);
                    break;

                case MINUTES_SECONDS:
                    disp.displayClock(minutes, seconds);
                    break;
                }
            }
            break;

        case _100P:
            data.changeTimeMode = MINUTES;
            memory.updateNow();
            break;

        case _200P:
            data.changeTimeMode = HOURS;
            memory.updateNow();
            break;

        case _0:
            data.changeTimeMode = SECONDS;
            memory.updateNow();
            break;

        case _1:
            // Падіння цифр
            data.changeNumMode = MODE_FALL;
            memory.updateNow();
            break;

        case _2:
            // Скрутка цифр
            data.changeNumMode = MODE_TWIST;
            memory.updateNow();
            break;

        case _3:
            // Звичайна зміна цифр
            data.changeNumMode = MODE_NORMAL;
            memory.updateNow();
            break;

        case _4:
            break;

        case _5:
            break;

        case _6:
            break;

        case _7:
            break;

        case _8:
            break;

        case _9:
            if (data.led_tick)
                digitalWrite(LED_BUILTIN, 0);
            data.led_tick = !data.led_tick;
            memory.updateNow();
            break;
        }
    }
}

// Для роботи енкодера
void checkEnc()
{
    enc.tick();

    if (enc.turn())
    {
        if (enc.left())
        {
            minusTime(false, true);
        }
        else if (enc.right())
        {
            plusTime(false, true);
        }
        tone(BUZZER, 1300, 60);
    }
    else if (enc.turnH())
    {
        if (enc.leftH())
        {
            minusTime(true);
        }
        else if (enc.rightH())
        {
            plusTime(true);
        }
        tone(BUZZER, 1000, 60);
    }
    else if (enc.held())
    {
        switch (data.showClockMode)
        {
        case HOURS_MINUTES:
            data.showClockMode = MINUTES_SECONDS;
            disp.displayClockScroll(minutes, seconds, 50);
            memory.updateNow();
            break;

        case 2:
            data.showClockMode = HOURS_MINUTES;
            disp.displayClockScroll(hours, minutes, 50);
            memory.updateNow();
            break;
        }
        tone(BUZZER, 1300, 70);
    }
}

// Для роботи ІК приймача
void irIsr()
{
    ir.tick();
}

// Як додавати час в залежності від режиму
void plusTimeWithMode()
{
    switch (data.changeTimeMode)
    {
    case 0:
        plusTime(true);
        break;
    case 1:
        plusTime(false, true);
        break;
    case 2:
        plusTime(false, false, true);
        break;
    default:
        plusTime(false, false, true);
        break;
    }
}

// Як віднімати час в залежності від режиму
void minusTimeWithMode()
{
    switch (data.changeTimeMode)
    {
    case 0:
        minusTime(true);
        break;
    case 1:
        minusTime(false, true);
        break;
    case 2:
        minusTime(false, false, true);
        break;
    default:
        minusTime(false, false, true);
        break;
    }
}

// Відобразити режим часів (год:хв/хв:сек) ОДРАЗУ
void displayClockWithMode()
{
    if (inAlarmMenu)
    {
        disp.displayClock(data.alarmH, data.alarmM);
    }
    else
        switch (data.showClockMode)
        {
        case HOURS_MINUTES:
            disp.displayClock(hours, minutes);
            break;

        case MINUTES_SECONDS:
            disp.displayClock(minutes, seconds);
            break;

        default:
            disp.clear();
            break;
        }
}

// Логіка оновлення дисплея
void displayUpdate()
{
    static bool point;
    point = !point;

    if (data.led_tick)
    {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
    disp.point(point);

    if (inAlarmMenu)
    {
        switch (showAlarmClockMode)
        {
        case HOURS_MINUTES:
            disp.displayClock(data.alarmH, data.alarmM);
            break;

        case MINUTES_SECONDS:
            disp.displayClock(data.alarmM, data.alarmS);
            break;
        }
    }
    else
    {
        switch (data.changeNumMode)
        {
        // Звичайна зміна цифр
        case MODE_NORMAL:
            switch (data.showClockMode)
            {
            case HOURS_MINUTES:
                disp.displayClock(hours, minutes);
                break;

            case MINUTES_SECONDS:
                disp.displayClock(minutes, seconds);
                break;
            }
            break;
        // Падіння цифр
        case MODE_FALL:
            switch (data.showClockMode)
            {
            case HOURS_MINUTES:
                disp.displayClockScroll(hours, minutes, 75);
                break;

            case MINUTES_SECONDS:
                disp.displayClockScroll(minutes, seconds, 75);
                break;
            }
            break;
        // Скрутка цифр
        case MODE_TWIST:
            switch (data.showClockMode)
            {
            case HOURS_MINUTES:
                disp.displayClockTwist(hours, minutes, 20);
                break;

            case MINUTES_SECONDS:
                disp.displayClockTwist(minutes, seconds, 20);
                break;
            }
            break;
        }
    }
}

// Перевірка чи час не вийшов за межі
void checkPlusTime()
{
    if (seconds > 59)
    {
        minutes++;
        seconds = 0;
    }
    if (minutes > 59)
    {
        tone(BUZZER, 1400, 450); //? Звук якщо 1 час минув
        hours++;
        minutes = 0;
    }
    if (hours > 23)
    {
        hours = 0;
    }
}

// Перевірка чи TIME < 0
void checkMinusTime()
{
    if (seconds < 0)
    {
        minutes--;
        seconds = 59;
    }
    if (minutes < 0)
    {
        hours--;
        minutes = 59;
    }
    if (hours < 0)
    {
        hours = 23;
    }
}

// Зменшити яскравість
void brightnessMinus()
{
    if (data.brightness != MIN_BRIGHTNESS)
    {
        data.brightness--;
        disp.brightness(data.brightness);
        memory.updateNow();
    }
    noTone(BUZZER);
}

// Збільшити яскравість
void brightnessPlus()
{
    if (data.brightness != MAX_BRIGHTNESS)
    {
        data.brightness++;
        disp.brightness(data.brightness);
        memory.updateNow();
    }
    noTone(BUZZER);
}

// Чи настав час будильника
void checkAlarmClock(int8_t Ahours, int8_t Aminutes)
{
    if (seconds == 0 and minutes == Aminutes and hours == Ahours)
    {
        tone(BUZZER, 900);
    }
}

// Перевірка додавання
void checkPlusAlarmTime()
{
    if (data.alarmS > 59)
    {
        data.alarmM++;
        data.alarmS = 0;
    }
    if (data.alarmM > 59)
    {
        data.alarmH++;
        data.alarmM = 0;
    }
    if (data.alarmH > 23)
    {
        data.alarmH = 0;
    }
}

// Перевірка віднімання часу будильника
void checkMinusAlarmTime()
{
    if (data.alarmS < 0)
    {
        data.alarmM--;
        data.alarmS = 59;
    }
    if (data.alarmM < 0)
    {
        data.alarmH--;
        data.alarmM = 59;
    }
    if (data.alarmH < 0)
    {
        data.alarmH = 23;
    }
}
