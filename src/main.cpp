#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/widgets/stepped_slider.h>
#include <gui/smgui.h>

#include <thread>

#include <mirisdr.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "mirisdr_source",
    /* Description:     */ "Mirisdr source module for SDR++",
    /* Author:          */ "cropinghigh",
    /* Version:         */ 0, 1, 2,
    /* Max instances    */ 1
};

ConfigManager config;

const char* sampleRatesTxt = "15MHz\00014MHz\00013MHz\00012MHz\00011MHz\00010MHz\0009MHz\0008MHz\0007MHz\0006MHz\0005MHz\0004MHz\0003MHz\0002MHz\0001.54MHz\000";

const int sampleRates[] = {
    15000000,
    14000000,
    13000000,
    12000000,
    11000000,
    10000000,
    9000000,
    8000000,
    7000000,
    6000000,
    5000000,
    4000000,
    3000000,
    2000000,
    1540000,
};

const char* bandwidthsTxt = "200kHz\0"
                            "300kHz\0"
                            "600kHz\0"
                            "1.536MHz\0"
                            "5MHz\0"
                            "6MHz\0"
                            "7MHz\0"
                            "8MHz\0";

const int bandwidths[] = {
    200000,
    300000,
    600000,
    1536000,
    5000000,
    6000000,
    7000000,
    8000000
};

const char* ifFreqsTxt = "0\0"
                         "450kHz\0"
                         "1.62MHz\0"
                         "2.048MHz\0";

const int ifFreqs[] = {
    0,
    450000,
    1620000,
    2048000,
};

class MirisdrSourceModule : public ModuleManager::Instance {
public:
    MirisdrSourceModule(std::string name) {
        this->name = name;

        // TODO Select the last samplerate option
        sampleRate = 1540000;
        srId = 14;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        refresh();

        config.acquire();
        std::string confSerial = config.conf["device"];
        config.release();
        selectBySerial(confSerial);

        sigpath::sourceManager.registerSource("Mirisdr", &handler);
    }

    ~MirisdrSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("Mirisdr");
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

    void refresh() {
        devList.clear();
        devListTxt = "";

        int cnt = mirisdr_get_device_count();

        for(int i = 0; i < cnt; i++) {
            char manufact[256];
            char product[256];
            char serial[256];
            if(!mirisdr_get_device_usb_strings(i, manufact, product, serial)) {
                std::string name = std::string(manufact) + " " + std::string(product) + " " + std::string(serial);
                devList.push_back(name);
                devListTxt += name;
                devListTxt += '\0';
            }
        }
    }

    void selectFirst() {
        if (devList.size() != 0) {
            selectBySerial(devList[0]);
            return;
        }
        selectedSerial = "";
    }

    void selectBySerial(std::string serial) {
        if (std::find(devList.begin(), devList.end(), serial) == devList.end()) {
            selectFirst();
            return;
        }

        bool created = false;
        config.acquire();
        if (!config.conf["devices"].contains(serial)) {
            config.conf["devices"][serial]["sampleRate"] = 1540000;
            config.conf["devices"][serial]["bandwidth"] = 3;
            config.conf["devices"][serial]["devset_autogain"] = devset_autogain;
            config.conf["devices"][serial]["devset_gain"] = devset_gain;
            config.conf["devices"][serial]["devset_hwf"] = devset_hwf;
            config.conf["devices"][serial]["devset_iffreq"] = devset_iffreq;
            config.conf["devices"][serial]["devset_offsettuning"] = devset_offsettuning;
            // config.conf["devices"][serial]["devset_tuner_gain"] = 0;
            // config.conf["devices"][serial]["devset_tuner_gain_mode"] = 0;
            config.conf["devices"][serial]["devset_mixer_gain"] = devset_mixer_gain;
            config.conf["devices"][serial]["devset_mixbuffer_gain"] = devset_mixbuffer_gain;
            config.conf["devices"][serial]["devset_lna_gain"] = devset_lna_gain;
            config.conf["devices"][serial]["devset_baseband_gain"] = devset_baseband_gain;
            config.conf["devices"][serial]["devset_bias"] = devset_bias;
        }
        config.release(created);

        // Set default values
        srId = 14;
        sampleRate = 1540000;
        bwId = 3;

        // Load from config if available and validate
        if (config.conf["devices"][serial].contains("sampleRate")) {
            int psr = config.conf["devices"][serial]["sampleRate"];
            for (int i = 0; i < 14; i++) {
                if (sampleRates[i] == psr) {
                    sampleRate = psr;
                    srId = i;
                }
            }
        }
        if (config.conf["devices"][serial].contains("bandwidth")) {
            bwId = config.conf["devices"][serial]["bandwidth"];
            bwId = std::clamp<int>(bwId, 0, 7);
        }
        if (config.conf["devices"][serial].contains("devset_autogain")) {
            devset_autogain = config.conf["devices"][serial]["devset_autogain"];
        }
        if (config.conf["devices"][serial].contains("devset_gain")) {
            devset_gain = config.conf["devices"][serial]["devset_gain"];
        }
        if (config.conf["devices"][serial].contains("devset_hwf")) {
            devset_hwf = config.conf["devices"][serial]["devset_hwf"];
        }
        if (config.conf["devices"][serial].contains("devset_iffreq")) {
            devset_iffreq = config.conf["devices"][serial]["devset_iffreq"];
            devset_iffreq = std::clamp<int>(devset_iffreq, 0, 3);
        }
        if (config.conf["devices"][serial].contains("devset_offsettuning")) {
            devset_offsettuning = config.conf["devices"][serial]["devset_offsettuning"];
        }
        // if (config.conf["devices"][serial].contains("devset_tuner_gain")) {
        //     devset_tuner_gain = config.conf["devices"][serial]["devset_tuner_gain"];
        // }
        // if (config.conf["devices"][serial].contains("devset_tuner_gain_mode")) {
        //     devset_tuner_gain_mode = config.conf["devices"][serial]["devset_tuner_gain_mode"];
        // }
        if (config.conf["devices"][serial].contains("devset_mixer_gain")) {
            devset_mixer_gain = config.conf["devices"][serial]["devset_mixer_gain"];
        }
        if (config.conf["devices"][serial].contains("devset_mixbuffer_gain")) {
            devset_mixbuffer_gain = config.conf["devices"][serial]["devset_mixbuffer_gain"];
        }
        if (config.conf["devices"][serial].contains("devset_lna_gain")) {
            devset_lna_gain = config.conf["devices"][serial]["devset_lna_gain"];
        }
        if (config.conf["devices"][serial].contains("devset_baseband_gain")) {
            devset_baseband_gain = config.conf["devices"][serial]["devset_baseband_gain"];
        }
        if (config.conf["devices"][serial].contains("devset_bias")) {
            devset_bias = config.conf["devices"][serial]["devset_bias"];
        }

        selectedSerial = serial;
    }

private:
    static void menuSelected(void* ctx) {
        MirisdrSourceModule* _this = (MirisdrSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("MirisdrSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        MirisdrSourceModule* _this = (MirisdrSourceModule*)ctx;
        flog::info("MirisdrSourceModule '{0}': Menu Deselect!", _this->name);
    }

    int bandwidthIdToBw(int id) {
        return bandwidths[id];
    }

    static void start(void* ctx) {
        MirisdrSourceModule* _this = (MirisdrSourceModule*)ctx;
        if (_this->running) { return; }
        if (_this->selectedSerial == "") {
            flog::error("Tried to start Mirisdr source with empty serial");
            return;
        }
        int cnt = mirisdr_get_device_count();
        int id = -1;

        for(int i = 0; i < cnt; i++) {
            char manufact[256];
            char product[256];
            char serial[256];
            if(!mirisdr_get_device_usb_strings(i, manufact, product, serial)) {
                std::string name = std::string(manufact) + " " + std::string(product) + " " + std::string(serial);
                if(name == _this->selectedSerial) {
                    id = i;
                }
            }
        }
        if(id == -1) {
            flog::error("Mirisdr device is not available");
            return;
        }

        if(mirisdr_open(&_this->openDev, id)) {
            flog::error("Could not open Mirisdr {0} id {1} cnt {2}", _this->selectedSerial, id, cnt);
            return;
        }
        if(mirisdr_set_hw_flavour(_this->openDev, _this->devset_hwf ? MIRISDR_HW_SDRPLAY : MIRISDR_HW_DEFAULT)) {
            flog::error("Could not set Mirisdr hw flavour {0}", _this->selectedSerial);
            return;
        }
        if(mirisdr_set_sample_format(_this->openDev, "AUTO")) {
            flog::error("Could not set Mirisdr sample format {0}", _this->selectedSerial);
            return;
        }
        if(mirisdr_set_transfer(_this->openDev, "BULK")) {
            flog::error("Could not set Mirisdr transfer {0}", _this->selectedSerial);
            return;
        }
        if(mirisdr_set_sample_rate(_this->openDev, _this->sampleRate)) {
            flog::error("Could not set Mirisdr sample rate {0}", _this->selectedSerial);
            return;
        }
        if(mirisdr_set_bandwidth(_this->openDev, _this->bandwidthIdToBw(_this->bwId))) {
            flog::error("Could not set Mirisdr bandwidth {0}", _this->selectedSerial);
            return;
        }
        if(mirisdr_set_center_freq(_this->openDev, _this->freq)) {
            flog::error("Could not set Mirisdr center freq {0}", _this->selectedSerial);
            return;
        }
        if(mirisdr_set_offset_tuning(_this->openDev, _this->devset_offsettuning)) {
                flog::error("Could not set Mirisdr offset tuning {0}", _this->selectedSerial);
                return;
            }
        if(!_this->devset_offsettuning) {
            if(mirisdr_set_if_freq(_this->openDev, ifFreqs[_this->devset_iffreq])) {
                flog::error("Could not set Mirisdr if freq {0}", _this->selectedSerial);
                return;
            }
        }
        if(_this->devset_autogain) {
            if(mirisdr_set_tuner_gain(_this->openDev, _this->devset_gain)) {
                flog::error("Could not set Mirisdr gain {0}", _this->selectedSerial);
                return;
            }
        } else {
            if(mirisdr_set_mixer_gain(_this->openDev, _this->devset_mixer_gain)) {
                flog::error("Could not set Mirisdr mixer gain {0}", _this->selectedSerial);
                return;
            }
            if(mirisdr_set_mixbuffer_gain(_this->openDev, _this->devset_mixbuffer_gain)) {
                flog::error("Could not set Mirisdr mixbuffer gain {0}", _this->selectedSerial);
                return;
            }
            if(mirisdr_set_lna_gain(_this->openDev, _this->devset_lna_gain)) {
                flog::error("Could not set Mirisdr lna gain {0}", _this->selectedSerial);
                return;
            }
            if(mirisdr_set_baseband_gain(_this->openDev, _this->devset_baseband_gain)) {
                flog::error("Could not set Mirisdr baseband gain {0}", _this->selectedSerial);
                return;
            }
        }
        if(mirisdr_set_bias(_this->openDev, _this->devset_bias)) {
            flog::error("Could not set Mirisdr bias {0}", _this->selectedSerial);
            return;
        }

        /* Reset endpoint before we start reading from it (mandatory) */
        if(mirisdr_reset_buffer(_this->openDev)) {
            flog::error("Failed to reset Mirisdr buffer {0}", _this->selectedSerial);
            return;
        }

        _this->workerThread = std::thread(mirisdr_read_async, _this->openDev, callback, _this, 0, (_this->sampleRate/50)*sizeof(int16_t));

        _this->running = true;

        flog::info("MirisdrSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        MirisdrSourceModule* _this = (MirisdrSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        if(mirisdr_cancel_async(_this->openDev)) {
            flog::error("Mirisdr async cancel failed {0}", _this->selectedSerial);
        }
        _this->stream.stopWriter();
        _this->workerThread.join();
        int err = mirisdr_close(_this->openDev);
        if (err) {
            flog::error("Could not close Mirisdr {0}", _this->selectedSerial);
        }
        _this->stream.clearWriteStop();
        flog::info("MirisdrSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        MirisdrSourceModule* _this = (MirisdrSourceModule*)ctx;
        if (_this->running) {
            if(mirisdr_set_center_freq(_this->openDev, (uint32_t)freq) || mirisdr_get_center_freq(_this->openDev) != (uint32_t)freq) {
                flog::error("Could not set Mirisdr freq {0}(selected {1}, current {2})", _this->selectedSerial, (uint32_t)freq, mirisdr_get_center_freq(_this->openDev));
            }
        }
        _this->freq = freq;
        flog::info("MirisdrSourceModule '{0}': Tune: {1}!", _this->name, (uint32_t)freq);
    }

    static void menuHandler(void* ctx) {
        MirisdrSourceModule* _this = (MirisdrSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_mirisdr_dev_sel_", _this->name), &_this->devId, _this->devListTxt.c_str())) {
            _this->selectBySerial(_this->devList[_this->devId]);
            config.acquire();
            config.conf["device"] = _this->selectedSerial;
            config.release(true);
        }

        if (SmGui::Combo(CONCAT("##_mirisdr_sr_sel_", _this->name), &_this->srId, sampleRatesTxt)) {
            _this->sampleRate = sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["sampleRate"] = _this->sampleRate;
            config.release(true);
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_mirisdr_refr_", _this->name))) {
            _this->refresh();
            _this->selectBySerial(_this->selectedSerial);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (SmGui::Checkbox(CONCAT("SDRPLAY HWF##_mirisdr_hwf_", _this->name), &_this->devset_hwf)) {
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["devset_hwf"] = _this->devset_hwf;
            config.release(true);
        }

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("Bandwidth");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_mirisdr_bw_sel_", _this->name), &_this->bwId, bandwidthsTxt)) {
            if (_this->running) {
                if(mirisdr_set_bandwidth(_this->openDev, _this->bandwidthIdToBw(_this->bwId))) {
                    flog::error("Could not set Mirisdr bandwidth {0}", _this->selectedSerial);
                }
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["bandwidth"] = _this->bwId;
            config.release(true);
        }

        if (SmGui::Checkbox(CONCAT("Offset tuning##_mirisdr_ost_", _this->name), &_this->devset_offsettuning)) {
            if (_this->running) {
                if(mirisdr_set_offset_tuning(_this->openDev, _this->devset_offsettuning)) {
                    flog::error("Could not set Mirisdr offset tuning {0}", _this->selectedSerial);
                }
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["devset_offsettuning"] = _this->devset_offsettuning;
            config.release(true);
        }

        if(_this->devset_offsettuning) { SmGui::BeginDisabled(); }
        SmGui::LeftLabel("IF Freq");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_mirisdr_iffreq_", _this->name), &_this->devset_iffreq, ifFreqsTxt)) {
            if (_this->running) {
                if(mirisdr_set_if_freq(_this->openDev, ifFreqs[_this->devset_iffreq])) {
                    flog::error("Could not set Mirisdr if freq {0}", _this->selectedSerial);
                }
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["devset_iffreq"] = _this->devset_iffreq;
            config.release(true);
        }
        if(_this->devset_offsettuning) { SmGui::EndDisabled(); }

        if (SmGui::Checkbox(CONCAT("Bias##_mirisdr_bias_", _this->name), &_this->devset_bias)) {
            if (_this->running) {
                if(mirisdr_set_bias(_this->openDev, _this->devset_bias)) {
                    flog::error("Could not set Mirisdr bias {0}", _this->selectedSerial);
                }
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["devset_bias"] = _this->devset_bias;
            config.release(true);
        }

        if (SmGui::Checkbox(CONCAT("Simple gain##_mirisdr_bias_", _this->name), &_this->devset_autogain)) {
            if (_this->running) {
                if(_this->devset_autogain) {
                    if(mirisdr_set_tuner_gain(_this->openDev, _this->devset_gain)) {
                        flog::error("Could not set Mirisdr gain {0}", _this->selectedSerial);
                    }
                } else {
                    if(mirisdr_set_mixer_gain(_this->openDev, _this->devset_mixer_gain)) {
                        flog::error("Could not set Mirisdr mixer gain {0}", _this->selectedSerial);
                    }
                    if(mirisdr_set_mixbuffer_gain(_this->openDev, _this->devset_mixbuffer_gain)) {
                        flog::error("Could not set Mirisdr mixbuffer gain {0}", _this->selectedSerial);
                    }
                    if(mirisdr_set_lna_gain(_this->openDev, _this->devset_lna_gain)) {
                        flog::error("Could not set Mirisdr lna gain {0}", _this->selectedSerial);
                    }
                    if(mirisdr_set_baseband_gain(_this->openDev, _this->devset_baseband_gain)) {
                        flog::error("Could not set Mirisdr baseband gain {0}", _this->selectedSerial);
                    }
                }
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["devset_autogain"] = _this->devset_autogain;
            config.release(true);
        }
        
        if(_this->devset_autogain) {
            SmGui::LeftLabel("Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_mirisdr_gain_", _this->name), &_this->devset_gain, 0, 102)) {
                if (_this->running) {
                    if(mirisdr_set_tuner_gain(_this->openDev, _this->devset_gain)) {
                        flog::error("Could not set Mirisdr gain {0}", _this->selectedSerial);
                    }
                }
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["devset_gain"] = (int)_this->devset_gain;
                config.release(true);
            }
        } else {
            SmGui::LeftLabel("Mixer gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_mirisdr_mgain_", _this->name), &_this->devset_mixer_gain, 0, 1)) {
                if (_this->running) {
                    if(mirisdr_set_mixer_gain(_this->openDev, _this->devset_mixer_gain)) {
                        flog::error("Could not set Mirisdr mixer gain {0}", _this->selectedSerial);
                    }
                }
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["devset_mixer_gain"] = (int)_this->devset_mixer_gain;
                config.release(true);
            }
            SmGui::LeftLabel("Mixbuffer gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_mirisdr_mbgain_", _this->name), &_this->devset_mixbuffer_gain, 0, 3)) {
                if (_this->running) {
                    if(mirisdr_set_mixbuffer_gain(_this->openDev, _this->devset_mixbuffer_gain)) {
                        flog::error("Could not set Mirisdr mixbuffer gain {0}", _this->selectedSerial);
                    }
                }
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["devset_mixbuffer_gain"] = (int)_this->devset_mixbuffer_gain;
                config.release(true);
            }
            SmGui::LeftLabel("LNA gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_mirisdr_lgain_", _this->name), &_this->devset_lna_gain, 0, 1)) {
                if (_this->running) {
                    if(mirisdr_set_lna_gain(_this->openDev, _this->devset_lna_gain)) {
                        flog::error("Could not set Mirisdr lna gain {0}", _this->selectedSerial);
                    }
                }
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["devset_lna_gain"] = (int)_this->devset_lna_gain;
                config.release(true);
            }
            SmGui::LeftLabel("Baseband gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_mirisdr_bbgain_", _this->name), &_this->devset_baseband_gain, 0, 59)) {
                if (_this->running) {
                    if(mirisdr_set_baseband_gain(_this->openDev, _this->devset_baseband_gain)) {
                        flog::error("Could not set Mirisdr baseband gain {0}", _this->selectedSerial);
                    }
                }
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["devset_baseband_gain"] = (int)_this->devset_baseband_gain;
                config.release(true);
            }
        }
    }

    static void callback(unsigned char *buf, uint32_t len, void *ctx) {
        MirisdrSourceModule* _this = (MirisdrSourceModule*)ctx;
        int count = (len/sizeof(int16_t)) / 2;
        int16_t* buffer = (int16_t*)buf;
        volk_16i_s32f_convert_32f((float*)_this->stream.writeBuf, buffer, 32768.0f, count * 2);
        if (!_this->stream.swap(count)) { return; }
    }

    std::string name;
    mirisdr_dev_t* openDev;
    bool enabled = true;
    std::thread workerThread;
    dsp::stream<dsp::complex_t> stream;
    int sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    std::string selectedSerial = "";
    int devId = 0;
    int srId = 0;
    int bwId = 16;
    bool devset_autogain = true;
    int devset_gain = 0;
    bool devset_hwf = false;
    int devset_iffreq = 0;
    bool devset_offsettuning = false;
    int devset_tuner_gain = 0;
    int devset_tuner_gain_mode = 0;
    int devset_mixer_gain = 0;
    int devset_mixbuffer_gain = 0;
    int devset_lna_gain = 0;
    int devset_baseband_gain = 0;
    bool devset_bias = 0;

    std::vector<std::string> devList;
    std::string devListTxt;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/mirisdr_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new MirisdrSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (MirisdrSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
