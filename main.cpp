#include <iostream>
#include <fstream>
#include <wiringPi.h>
#include <config.h>
#include <unistd.h>
#include <csignal>
#include <sys/reboot.h>


#define FANON LOW
#define FANOFF HIGH
#define PWM_REVERSE
//#define USE_SOFT_PWM
//#define DEBUG

#ifdef USE_SOFT_PWM

#include <softPwm.h>

#endif

bool isRun = true;
const char *configFile = "/etc/fanControl/config.conf";

typedef struct CommonConfig {
    unsigned int detectionInterval;
} CommonConfig;

typedef struct FanConfig {
    bool isEnable;
    int fanGPIO;
    int fanOnTemp;
    int fanOffTemp;
} FanConfig;

typedef struct PWMFanConfig {
    bool isEnable;
    int pwmGPIO;
    int fanOnTemp;
    int fanMinPWM;
    int fanMaxTemp;
    int fanMaxPWM;
    int fanOffTemp;
} PWMFanConfig;

typedef struct CpuFreqConfig {
    bool isEnable{};
    int highTemp{};
    std::string highTempGovernor;
    int lowTemp{};
    std::string lowTempGovernor{};
} CpuFreqConfig;

typedef struct RebootConfig {
    bool isEnable;
    int rebootTemp;
} RebootConfig;

typedef struct ShutdownConfig {
    bool isEnable;
    int shutdownTemp;
} ShutdownConfig;

typedef struct GlobalConfig {
    CommonConfig commonConfig{};
    FanConfig fanConfig{};
    PWMFanConfig pwmFanConfig{};
    CpuFreqConfig cpuFreqConfig;
    RebootConfig rebootConfig{};
    ShutdownConfig shutdownConfig{};
} GlobalConfig;

GlobalConfig globalConfig;


int getCpuTemp() {
    int cpuTemp;
    const char *cpuTempFile = "/sys/class/thermal/thermal_zone0/temp";
    std::ifstream ifs;
    ifs.open(cpuTempFile, std::fstream::in);
    ifs >> cpuTemp;
    ifs.close();
    return cpuTemp / 1000;
}

void setCpuGovernor(const char *governor) {
    const char *scalingGovernorFile = "/sys/devices/system/cpu/cpufreq/policy0/scaling_governor";
    std::ofstream ofs;
    ofs.open(scalingGovernorFile, std::fstream::out);
    ofs << governor;
    ofs.close();
}

void rebootSystem() {
    sync();
    setuid(0);
    if (reboot(RB_AUTOBOOT) == -1) {
        std::cout << "Reboot Fail" << std::endl;
    }
}

void shutdownSystem() {
    sync();
    setuid(0);
    if (reboot(RB_POWER_OFF) == -1) {
        std::cout << "Shutdown Fail" << std::endl;
    }
}

void singHandler(int sig) {
    printf("Exit by sig %d", sig);
    isRun = false;
}

void registerSingHandler() {
    signal(SIGINT, singHandler);
    signal(SIGKILL, singHandler);
}

void loadConfig() {
    rude::Config config;
    if (not config.load(configFile)) {
        std::cout << "Error loading config file: " << config.getError() << std::endl;
        exit(1);
    }

    if (not config.setSection("common")) {
        std::cout << "Error loading config file in \"common\"" << std::endl;
        exit(1);
    }
    int detectionInterval = config.getIntValue("detectionInterval");
    if (detectionInterval <= 0) {
        std::cout << "Illegal configuration \"detectionInterval\"" << std::endl;
        exit(1);
    }
    globalConfig.commonConfig.detectionInterval = detectionInterval;

    if (not config.setSection("fan")) {
        std::cout << "Error loading config file in \"fan\"" << std::endl;
        exit(1);
    }
    globalConfig.fanConfig.isEnable = config.getBoolValue("enable");
    globalConfig.fanConfig.fanGPIO = config.getIntValue("fanGPIO");
    globalConfig.fanConfig.fanOnTemp = config.getIntValue("fanOnTemp");
    globalConfig.fanConfig.fanOffTemp = config.getIntValue("fanOffTemp");
    if (globalConfig.fanConfig.fanOffTemp > globalConfig.fanConfig.fanOnTemp) {
        std::cout << R"("fanOffTemp" must be less than or equal to "fanOnTemp")" << std::endl;
        exit(1);
    }

    if (not config.setSection("pwm_fan")) {
        std::cout << "Error loading config file in \"pwm_fan\"" << std::endl;
        exit(1);
    }
    globalConfig.pwmFanConfig.isEnable = config.getBoolValue("enable");
    globalConfig.pwmFanConfig.pwmGPIO = config.getIntValue("pwmGPIO");
    globalConfig.pwmFanConfig.fanOnTemp = config.getIntValue("fanOnTemp");
    globalConfig.pwmFanConfig.fanMinPWM = config.getIntValue("fanMinPWM");
    globalConfig.pwmFanConfig.fanMaxTemp = config.getIntValue("fanMaxTemp");
    globalConfig.pwmFanConfig.fanMaxPWM = config.getIntValue("fanMaxPWM");
    globalConfig.pwmFanConfig.fanOffTemp = config.getIntValue("fanOffTemp");
    if (globalConfig.pwmFanConfig.fanMinPWM > 100 or globalConfig.pwmFanConfig.fanMinPWM < 0
        or globalConfig.pwmFanConfig.fanMaxPWM > 100 or globalConfig.pwmFanConfig.fanMaxPWM < 0) {
        std::cout << "PWM must be between 0 and 100" << std::endl;
        exit(1);
    }
    if (globalConfig.pwmFanConfig.fanOffTemp > globalConfig.pwmFanConfig.fanOnTemp) {
        std::cout << R"("fanOffTemp" must be less than or equal to "fanOnTemp")" << std::endl;
        exit(1);
    }

    if (not config.setSection("cpu_freq")) {
        std::cout << "Error loading config file in \"cpu_freq\"" << std::endl;
        exit(1);
    }
    globalConfig.cpuFreqConfig.isEnable = config.getBoolValue("enable");
    globalConfig.cpuFreqConfig.highTemp = config.getIntValue("highTemp");
    globalConfig.cpuFreqConfig.highTempGovernor = std::basic_string(config.getStringValue("highTempGovernor"));
    globalConfig.cpuFreqConfig.lowTemp = config.getIntValue("lowTemp");
    globalConfig.cpuFreqConfig.lowTempGovernor = std::basic_string(config.getStringValue("lowTempGovernor"));
    if (globalConfig.cpuFreqConfig.lowTemp > globalConfig.cpuFreqConfig.highTemp) {
        std::cout << R"("lowTemp" must be less than or equal to "highTemp")" << std::endl;
        exit(1);
    }

    if (not config.setSection("reboot")) {
        std::cout << "Error loading config file in \"reboot\"" << std::endl;
        exit(1);
    }
    globalConfig.rebootConfig.isEnable = config.getBoolValue("enable");
    globalConfig.rebootConfig.rebootTemp = config.getIntValue("rebootTemp");

    if (not config.setSection("shutdown")) {
        std::cout << "Error loading config file in \"reboot\"" << std::endl;
        exit(1);
    }
    globalConfig.shutdownConfig.isEnable = config.getBoolValue("enable");
    globalConfig.shutdownConfig.shutdownTemp = config.getIntValue("shutdownTemp");
}

void fanController(int cpuTemp) {
    static int funStatic = 2;  // 0:off 1:on 2:unknown

    if (not globalConfig.fanConfig.isEnable) {
        return;
    }
    if (cpuTemp > globalConfig.fanConfig.fanOnTemp) {
        if (funStatic == 1) {
            return;
        }
        funStatic = 1;
#ifdef DEBUG
        printf("Fan on\n");
#endif
        digitalWrite(globalConfig.fanConfig.fanGPIO, FANON);
        return;
    }
    if (cpuTemp < globalConfig.fanConfig.fanOffTemp) {
        if (funStatic == 0) {
            return;
        }
        funStatic = 0;
#ifdef DEBUG
        printf("Fan off\n");
#endif
        digitalWrite(globalConfig.fanConfig.fanGPIO, FANOFF);
        return;
    }
}

void pwmFanController(int cpuTemp) {
    if (not globalConfig.pwmFanConfig.isEnable) {
        return;
    }
    int pwm = 0;
    if (cpuTemp >= globalConfig.pwmFanConfig.fanOnTemp) {
#ifdef DEBUG
        printf("PWMFan on\n");
#endif
        int diff = globalConfig.pwmFanConfig.fanMaxTemp - globalConfig.pwmFanConfig.fanOnTemp;
        int step = (globalConfig.pwmFanConfig.fanMaxPWM - globalConfig.pwmFanConfig.fanMinPWM) / diff;
        pwm = step * (cpuTemp - globalConfig.pwmFanConfig.fanOnTemp) + globalConfig.pwmFanConfig.fanMinPWM;
    }
    if (cpuTemp < globalConfig.pwmFanConfig.fanOffTemp) {
#ifdef DEBUG
        printf("PWMFan off\n");
#endif
        pwm = 0;
    }
#ifdef PWM_REVERSE
    pwm = 1023 - pwm;
#endif

#ifdef USE_SOFT_PWM
    softPwmWrite(globalConfig.pwmFanConfig.pwmGPIO, pwm);
#else
    pwmWrite(globalConfig.pwmFanConfig.pwmGPIO, pwm);
#endif
}

void cpuFreqController(int cpuTemp) {
    static bool isGovernorSet = false;
    if (not globalConfig.cpuFreqConfig.isEnable) {
        return;
    }
    if (cpuTemp >= globalConfig.cpuFreqConfig.highTemp) {
        if (isGovernorSet) {
            return;
        }
        isGovernorSet = true;
#ifdef DEBUG
        printf("Set CPU governor: %s\n", globalConfig.cpuFreqConfig.highTempGovernor.c_str());
#endif
        setCpuGovernor(globalConfig.cpuFreqConfig.highTempGovernor.c_str());
        return;
    }
    if (cpuTemp < globalConfig.cpuFreqConfig.lowTemp) {
        if (!isGovernorSet) {
            return;
        }
        isGovernorSet = false;
#ifdef DEBUG
        printf("Set CPU governor: %s\n", globalConfig.cpuFreqConfig.lowTempGovernor.c_str());
#endif
        setCpuGovernor(globalConfig.cpuFreqConfig.lowTempGovernor.c_str());
        return;
    }
}

void rebootController(int cpuTemp) {
    if (not globalConfig.rebootConfig.isEnable) {
        return;
    }
    if (cpuTemp > globalConfig.rebootConfig.rebootTemp) {
#ifdef DEBUG
        printf("Reboot\n");
#endif
        rebootSystem();
    }
}

void shutdownController(int cpuTemp) {
    if (not globalConfig.shutdownConfig.isEnable) {
        return;
    }
    if (cpuTemp > globalConfig.shutdownConfig.shutdownTemp) {
#ifdef DEBUG
        printf("Shutdown\n");
#endif
        shutdownSystem();
    }
}

void gpioInit() {
    wiringPiSetup();
    if (globalConfig.fanConfig.isEnable) {
        pinMode(globalConfig.fanConfig.fanGPIO, OUTPUT);
    }
    if (globalConfig.pwmFanConfig.isEnable) {
#ifdef USE_SOFT_PWM
        softPwmCreate(globalConfig.pwmFanConfig.pwmGPIO, 0, 1024);
#else
        pinMode(globalConfig.pwmFanConfig.pwmGPIO, PWM_OUTPUT);
#endif
    }
}

void setup() {
    registerSingHandler();
    loadConfig();
    gpioInit();
}

int main() {
#ifdef DEBUG
    printf("Start\n");
#endif
    setup();

    while (isRun) {
        int cpuTemp = getCpuTemp();
#ifdef DEBUG
        printf("CPU Temp: %d\n", cpuTemp);
#endif
        fanController(cpuTemp);
        pwmFanController(cpuTemp);
        cpuFreqController(cpuTemp);
        rebootController(cpuTemp);
        shutdownController(cpuTemp);

        sleep(globalConfig.commonConfig.detectionInterval);
    }
    return 0;
}