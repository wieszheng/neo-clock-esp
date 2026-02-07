#include "DisplayManager.h"
#include "Apps.h"
#include "Tools.h"
#include <ArduinoJson.h>

CRGB leds[NUM_LEDS];

FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, 8, 8, 4, 1, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE);
MatrixDisplayUi *ui = new MatrixDisplayUi(matrix);

DisplayManager_ &DisplayManager_::getInstance()
{
    static DisplayManager_ instance;
    return instance;
}

DisplayManager_ &DisplayManager = DisplayManager.getInstance();

void DisplayManager_::setup()
{
    FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, MATRIX_WIDTH * MATRIX_HEIGHT);
    setMatrixLayout(MATRIX_LAYOUT);

    // player.setMatrix(matrix);

    ui->setAppAnimation(SLIDE_DOWN);
    ui->setTargetFPS(MATRIX_FPS);
    ui->setTimePerApp(TIME_PER_APP);
    ui->setTimePerTransition(TIME_PER_TRANSITION);
    // ui->setOverlays(overlays, 4);
    ui->init();
}

void DisplayManager_::setBrightness(uint8_t bri)
{
    if (MATRIX_OFF)
    {
        matrix->setBrightness(0);
    }
    else
    {
        matrix->setBrightness(bri);
    }
}

void DisplayManager_::setTextColor(uint16_t color)
{
    matrix->setTextColor(color);
}

void DisplayManager_::setMatrixState(bool on)
{
    MATRIX_OFF = !on;
    setBrightness(BRIGHTNESS);
}

void DisplayManager_::defaultTextColor()
{
    setTextColor(TEXTCOLOR_565);
}

void DisplayManager_::setMatrixLayout(int layout)
{
    delete matrix;

    switch (layout)
    {
    case 0:
        matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
        break;

    case 2:
        matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
        break;

    case 3:
        matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_BOTTOM + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE);
        break;

    case 4:
        matrix = new FastLED_NeoMatrix(leds, 8, 8, 4, 1, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
        break;

    case 5:
        matrix = new FastLED_NeoMatrix(leds, 8, 8, 4, 1, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE);
    default:
        break;
    }

    delete ui;
    ui = new MatrixDisplayUi(matrix);
}

void DisplayManager_::tick()
{
    if (AP_MODE)
    {
    }
    else
    {
        ui->update();
    }
}

void DisplayManager_::clear()
{
    matrix->clear();
}

void DisplayManager_::show()
{
    matrix->show();
}

void DisplayManager_::printText(int16_t x, int16_t y, const char *text, bool centered, bool ignoreUppercase)
{

    if (centered)
    {
        uint16_t textWidth = getTextWidth(text, ignoreUppercase);
        int16_t textX = (MATRIX_WIDTH - textWidth) / 2;
        matrix->setCursor(textX, y);
    }
    else
    {
        matrix->setCursor(x, y);
    }

    if (!ignoreUppercase)
    {
        size_t length = strlen(text);
        char upperText[length + 1];

        for (size_t i = 0; i < length; ++i)
        {
            upperText[i] = toupper(text[i]);
        }

        upperText[length] = '\0';
        matrix->print(upperText);
    }
    else
    {
        matrix->print(text);
    }
}

void DisplayManager_::applyAllSettings()
{
    ui->setTargetFPS(MATRIX_FPS);
    ui->setTimePerApp(TIME_PER_APP);
    ui->setTimePerTransition(TIME_PER_TRANSITION);
    setBrightness(BRIGHTNESS);
    setTextColor(TEXTCOLOR_565);

    if (AUTO_TRANSITION)
    {
        ui->enablesetAutoTransition();
    }
    else
    {
        ui->disablesetAutoTransition();
    }
}

void DisplayManager_::loadNativeApps()
{
    Apps.clear();

    Apps.push_back({"time", TimeApp, SHOW_TIME, TIME_POSITION, TIME_DURATION});
    Apps.push_back({"date", DateApp, SHOW_DATE, DATE_POSITION, DATE_DURATION});
    // Apps.push_back({"temp", TempApp, SHOW_TEMP, TEMP_POSITION, TEMP_DURATION});
    // Apps.push_back({"hum", HumApp, SHOW_HUM, HUM_POSITION, HUM_DURATION});
    // Apps.push_back({"weather", WeatherApp, SHOW_WEATHER, WEATHER_POSITION, WEATHER_DURATION});
    // Apps.push_back({"wind", WindApp, SHOW_WIND, WIND_POSITION, WIND_DURATION});

    std::sort(Apps.begin(), Apps.end(), [](const AppData &a, const AppData &b)
              { return a.position < b.position; });

    ui->setApps(Apps);
}

void DisplayManager_::updateAppVector(const char *json)
{
    DynamicJsonDocument doc(1024);
    auto error = deserializeJson(doc, json);
    if (error)
        return;

    for (const auto &app : doc.as<JsonArray>())
    {
        String name = app["name"].as<String>();
        bool show = app.containsKey("show") ? app["show"].as<bool>() : true;
        int position = app.containsKey("pos") ? app["pos"].as<int>() : -1;

        if (name == "time")
            SHOW_TIME = show;
        else if (name == "date")
            SHOW_DATE = show;
        else if (name == "temp")
            SHOW_TEMP = show;
        else if (name == "hum")
            SHOW_HUM = show;
        else if (name == "weather")
            SHOW_WEATHER = show;
    }

    loadNativeApps();
}

void DisplayManager_::setNewSettings(const char *json)
{
    DynamicJsonDocument doc(512);
    auto error = deserializeJson(doc, json);
    if (error)
        return;

    TIME_PER_APP = doc.containsKey("appTime") ? doc["appTime"].as<int>() : TIME_PER_APP;
    TIME_PER_TRANSITION = doc.containsKey("transition") ? doc["transition"].as<int>() : TIME_PER_TRANSITION;
    BRIGHTNESS = doc.containsKey("brightness") ? doc["brightness"].as<int>() : BRIGHTNESS;
    MATRIX_FPS = doc.containsKey("fps") ? doc["fps"].as<int>() : MATRIX_FPS;
    AUTO_TRANSITION = doc.containsKey("autoTransition") ? doc["autoTransition"].as<bool>() : AUTO_TRANSITION;

    applyAllSettings();
}

void DisplayManager_::nextApp()
{
    ui->nextApp();
}

void DisplayManager_::previousApp()
{
    ui->previousApp();
}

void DisplayManager_::leftButton()
{
    ui->previousApp();
}

void DisplayManager_::rightButton()
{
    ui->nextApp();
}