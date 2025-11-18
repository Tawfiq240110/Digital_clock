// Build: g++ clock_mouse_menu.cpp -lopengl32 -lfreeglut -lglu32 -std=c++11
#include <GL/glut.h>
#include <ctime>
#include <cmath>
#include <string>
#include <iostream>
#include <windows.h>
using namespace std;

class Clock {
public:
    // ===== Alarm settings =====
    static int  alarmHour;
    static int  alarmMinute;
    static bool alarmEnabled;

    // ===== Reminder settings =====
    static int  reminderHour;
    static int  reminderMinute;
    static bool reminderEnabled;

    // ===== UI state =====
    enum UIState { UI_MAIN, UI_MENU_OPEN, UI_SET_REM };
    static UIState uiState;

    // window size (for mouse coord mapping)
    static int winW, winH;

    // selection buffers for Set Reminder screen
    static int selHour, selMin;

    // simple button
    struct Button {
        float x1, y1, x2, y2; // in world coords (-1..1)
        string label;
        bool visible = false;
    };

    // right panel bounds
    static constexpr float PANEL_X1 = 0.55f;
    static constexpr float PANEL_X2 = 0.98f;
    static constexpr float PANEL_Y1 = 0.90f;
    static constexpr float PANEL_Y2 = -0.90f;

    // buttons
    static Button btnMenu;
    static Button btnSetRem;
    static Button btnBack;

    static Button btnHPlus, btnHMinus;
    static Button btnMPlus, btnMMinus;
    static Button btnSave, btnEnable, btnDisable;

    Clock() { cout << "Clock started\n"; }
    ~Clock() { cout << "Clock stopped\n"; }

    // -------- drawing helpers --------
    static void drawText(float x, float y, void* font, const char* text) {
        glRasterPos2f(x, y);
        while (*text) { glutBitmapCharacter(font, *text); ++text; }
    }
    static void drawRect(float x1, float y1, float x2, float y2, bool filled) {
        if (filled) glBegin(GL_QUADS); else glBegin(GL_LINE_LOOP);
        glVertex2f(x1, y1); glVertex2f(x2, y1);
        glVertex2f(x2, y2); glVertex2f(x1, y2);
        glEnd();
    }
    static void drawButton(const Button& b) {
        if (!b.visible) return;
        glColor3f(0.18f, 0.18f, 0.18f);
        drawRect(b.x1, b.y1, b.x2, b.y2, true);
        glColor3f(0.8f, 0.8f, 0.8f);
        drawRect(b.x1, b.y1, b.x2, b.y2, false);
        // center text
        float cx = (b.x1 + b.x2) * 0.5f;
        float cy = (b.y1 + b.y2) * 0.5f - 0.01f;
        glColor3f(1,1,1);
        drawText(cx - 0.0125f * b.label.size(), cy, GLUT_BITMAP_HELVETICA_18, b.label.c_str());
    }
    static bool pointIn(const Button& b, float x, float y) {
        if (!b.visible) return false;
        return (x >= b.x1 && x <= b.x2 && y <= b.y1 && y >= b.y2);
    }

    // -------- clock drawing --------
    static void drawHand(float angleDeg, float length, float thickness) {
        float angleRad = (-angleDeg + 90) * 3.14159265f / 180.0f;
        glLineWidth(thickness);
        glBegin(GL_LINES);
        glVertex2f(0, 0);
        glVertex2f(length * cos(angleRad), length * sin(angleRad));
        glEnd();
    }

    static void drawNumbers() {
        int numbers[13] = {0, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 12};
        for (int i = 1; i <= 12; ++i) {
            int num = numbers[i];
            float angle = (i * 30 + 90) * 3.14159265f / 180.0f;
            float radius = 0.43f;
            float x = cos(angle) * radius;
            float y = sin(angle) * radius;
            string numStr = to_string(num);
            glColor3f(1, 1, 1);
            drawText(x - 0.025f * numStr.length(), y - 0.02f, GLUT_BITMAP_HELVETICA_18, numStr.c_str());
        }
    }

    // -------- UI panels --------
    static void drawRightPanel() {
        // panel background
        glColor3f(0.10f, 0.10f, 0.10f);
        drawRect(PANEL_X1, PANEL_Y1, PANEL_X2, PANEL_Y2, true);
        glColor3f(0.35f, 0.35f, 0.35f);
        drawRect(PANEL_X1, PANEL_Y1, PANEL_X2, PANEL_Y2, false);

        // show header
        glColor3f(1,1,1);
        drawText(PANEL_X1 + 0.03f, PANEL_Y1 - 0.05f, GLUT_BITMAP_HELVETICA_18, "Menu Panel");

        // configure which buttons are visible for current state
        btnMenu.visible   = (uiState == UI_MAIN);
        btnSetRem.visible = (uiState == UI_MENU_OPEN);
        btnBack.visible   = (uiState == UI_MENU_OPEN || uiState == UI_SET_REM);

        btnHPlus.visible = btnHMinus.visible = btnMPlus.visible = btnMMinus.visible =
        btnSave.visible = btnEnable.visible = btnDisable.visible = (uiState == UI_SET_REM);

        // draw accordingly
        drawButton(btnMenu);
        drawButton(btnSetRem);
        drawButton(btnBack);

        drawButton(btnHPlus); drawButton(btnHMinus);
        drawButton(btnMPlus); drawButton(btnMMinus);
        drawButton(btnSave);  drawButton(btnEnable); drawButton(btnDisable);

        // show current selection or set values
        if (uiState == UI_SET_REM) {
            char buf[64];
            sprintf(buf, "Hour: %02d", selHour);
            drawText(PANEL_X1 + 0.05f, 0.20f, GLUT_BITMAP_HELVETICA_18, buf);
            sprintf(buf, "Min : %02d", selMin);
            drawText(PANEL_X1 + 0.05f, 0.12f, GLUT_BITMAP_HELVETICA_18, buf);

            drawText(PANEL_X1 + 0.05f, -0.10f, GLUT_BITMAP_HELVETICA_12,
                     "Tip: +/- দিয়ে সময় বদলান, Save চাপ");
        }
    }

    // -------- main display --------
    static void display() {
        glClear(GL_COLOR_BUFFER_BIT);

        // Current time
        time_t now = time(0);
        tm* localTime = localtime(&now);

        // 12-hour format
        int hour12 = localTime->tm_hour;
        bool isPM = false;
        if (hour12 >= 12) { isPM = true; if (hour12 > 12) hour12 -= 12; }
        if (hour12 == 0) hour12 = 12;

        char ampm[3]; strcpy(ampm, isPM ? "PM" : "AM");
        char timeStr[12];
        sprintf(timeStr, "%02d:%02d:%02d %s", hour12, localTime->tm_min, localTime->tm_sec, ampm);

        const char* days[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
        const char* dayStr = days[localTime->tm_wday];

        // Time & day
        glColor3f(1, 1, 1);
        drawText(-0.35f, 0.82f, GLUT_BITMAP_HELVETICA_18, timeStr);
        drawText(-0.20f, 0.72f, GLUT_BITMAP_HELVETICA_12, dayStr);

        // Reminder check
        if (reminderEnabled && localTime->tm_hour == reminderHour && localTime->tm_min == reminderMinute) {
            glColor3f(1, 1, 0);
            drawText(-0.15f, -0.80f, GLUT_BITMAP_HELVETICA_18, "Reminder!");
            // optional beep once
            static int lastBeepMin = -1;
            if (lastBeepMin != localTime->tm_min) {
                MessageBeep(MB_ICONEXCLAMATION);
                lastBeepMin = localTime->tm_min;
            }
        }

        // Alarm check (kept from your original)
        if (alarmEnabled && localTime->tm_hour == alarmHour && localTime->tm_min == alarmMinute) {
            glColor3f(1, 0, 0);
            drawText(-0.10f, -0.86f, GLUT_BITMAP_HELVETICA_18, "ALARM!!!");
            alarmEnabled = false; // one-shot
        }

        // Clock circle & hands
        glPushMatrix();
        glTranslatef(-0.20f, -0.05f, 0.0f); // move a bit left (because right panel exists)

        glColor3f(0.6f, 0.6f, 0.6f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 360; i++) {
            float theta = i * 3.14159265f / 180.0f;
            glVertex2f(cos(theta) * 0.5f, sin(theta) * 0.5f);
        }
        glEnd();

        drawNumbers();

        float hourAngle   = (localTime->tm_hour % 12 + localTime->tm_min / 60.0f) * 30.0f;
        float minuteAngle = (localTime->tm_min + localTime->tm_sec / 60.0f) * 6.0f;
        float secondAngle = localTime->tm_sec * 6.0f;

        glColor3f(1, 0, 0);   drawHand(hourAngle,   0.25f, 5);
        glColor3f(0, 1, 0);   drawHand(minuteAngle, 0.35f, 3);
        glColor3f(0, 0, 1);   drawHand(secondAngle, 0.45f, 1);

        glPopMatrix();

        // Right side menu / inputs
        drawRightPanel();

        glutSwapBuffers();
    }

    // -------- timer --------
    static void timer(int) {
        glutPostRedisplay();
        glutTimerFunc(250, timer, 0); // refresh faster for snappy UI
    }

    // -------- reshape --------
    static void reshape(int w, int h) {
        winW = w; winH = h;
        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(-1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    // -------- mouse handling --------
    static void mouse(int button, int state, int x, int y) {
        if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;

        // convert window pixels -> world coords (-1..1)
        float wx =  ( (float)x / (float)winW ) * 2.0f - 1.0f;
        float wy =  ( (float)(winH - y) / (float)winH ) * 2.0f - 1.0f;

        // which button was clicked?
        if (pointIn(btnMenu, wx, wy)) {
            uiState = UI_MENU_OPEN;
            return;
        }
        if (pointIn(btnBack, wx, wy)) {
            uiState = UI_MAIN;
            return;
        }
        if (pointIn(btnSetRem, wx, wy)) {
            uiState = UI_SET_REM;
            // prefill with current reminder time
            selHour = reminderHour;
            selMin  = reminderMinute;
            return;
        }

        if (uiState == UI_SET_REM) {
            if (pointIn(btnHPlus, wx, wy))  { selHour = (selHour + 1) % 24; return; }
            if (pointIn(btnHMinus, wx, wy)) { selHour = (selHour + 23) % 24; return; }
            if (pointIn(btnMPlus, wx, wy))  { selMin  = (selMin + 1) % 60;  return; }
            if (pointIn(btnMMinus, wx, wy)) { selMin  = (selMin + 59) % 60; return; }
            if (pointIn(btnEnable, wx, wy)) { reminderEnabled = true;  return; }
            if (pointIn(btnDisable, wx, wy)){ reminderEnabled = false; return; }
            if (pointIn(btnSave, wx, wy)) {
                reminderHour   = selHour;
                reminderMinute = selMin;
                reminderEnabled = true;
                cout << "Reminder saved: " << reminderHour << ":" << reminderMinute << " (enabled)\n";
                uiState = UI_MAIN;
                return;
            }
        }
    }

    static void initButtons() {
        // Layout helpers
        float xL = PANEL_X1 + 0.04f, xR = PANEL_X2 - 0.04f;
        float bw = (xR - xL);
        float h  = 0.09f;
        float gap= 0.02f;

        // Top level
        btnMenu   = { xL,  0.70f, xR, 0.70f - h, "Menu", false };
        btnSetRem = { xL,  0.50f, xR, 0.50f - h, "Set Reminder", false };
        btnBack   = { xL, -0.75f, xR, -0.75f - h, "Back", false };

        // Set Reminder controls
        float rowTop = 0.05f;
        btnHPlus  = { xL, rowTop,            xL + bw*0.45f - 0.01f, rowTop - h, "+Hour", false };
        btnHMinus = { xL + bw*0.55f + 0.01f, rowTop, xR,             rowTop - h, "-Hour", false };

        rowTop -= (h + gap);
        btnMPlus  = { xL, rowTop,            xL + bw*0.45f - 0.01f, rowTop - h, "+Min", false };
        btnMMinus = { xL + bw*0.55f + 0.01f, rowTop, xR,             rowTop - h, "-Min", false };

        rowTop -= (h + gap);
        btnEnable = { xL, rowTop, xL + bw*0.45f - 0.01f, rowTop - h, "Enable", false };
        btnDisable= { xL + bw*0.55f + 0.01f, rowTop, xR, rowTop - h, "Disable", false };

        rowTop -= (h + gap);
        btnSave  = { xL, rowTop, xR, rowTop - h, "Save", false };
    }

    // -------- start --------
    void start(int argc, char** argv) {
        glutInit(&argc, argv);
        glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
        glutInitWindowSize(700, 500);
        glutInitWindowPosition(100, 100);
        glutCreateWindow("Clock with Mouse Menu (Alarm & Reminder)");

        glClearColor(0, 0, 0, 1);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(-1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        initButtons();

        glutDisplayFunc(display);
        glutTimerFunc(250, timer, 0);
        glutReshapeFunc(reshape);
        glutMouseFunc(mouse);

        glutMainLoop();
    }
};

// -------- static initialization --------
int  Clock::alarmHour = 7;
int  Clock::alarmMinute = 30;
bool Clock::alarmEnabled = true;

int  Clock::reminderHour = 12;
int  Clock::reminderMinute = 20;
bool Clock::reminderEnabled = true;

Clock::UIState Clock::uiState = Clock::UI_MAIN;
int Clock::winW = 700;
int Clock::winH = 500;

int Clock::selHour = Clock::reminderHour;
int Clock::selMin  = Clock::reminderMinute;

Clock::Button Clock::btnMenu;
Clock::Button Clock::btnSetRem;
Clock::Button Clock::btnBack;
Clock::Button Clock::btnHPlus, Clock::btnHMinus;
Clock::Button Clock::btnMPlus, Clock::btnMMinus;
Clock::Button Clock::btnSave, Clock::btnEnable, Clock::btnDisable;

// -------- main --------
int main(int argc, char** argv) {
    Clock myClock;
    myClock.start(argc, argv);
    return 0;
}
/*
CODE COMMAND:    g++ digital_clock.cpp -o digital_clock.exe -lfreeglut -lopengl32 -lglu32
                 ./digital_clock.exe
*/