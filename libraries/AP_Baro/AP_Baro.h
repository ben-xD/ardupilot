#pragma once

#include "AP_Baro_config.h"

#include <AP_HAL/AP_HAL.h>
#include <AP_Param/AP_Param.h>
#include <AP_Math/AP_Math.h>
#include <Filter/DerivativeFilter.h>
#include <AP_MSP/msp.h>
#include <AP_ExternalAHRS/AP_ExternalAHRS.h>

// maximum number of sensor instances
#ifndef BARO_MAX_INSTANCES
#define BARO_MAX_INSTANCES 3
#endif

// maximum number of drivers. Note that a single driver can provide
// multiple sensor instances
#define BARO_MAX_DRIVERS 3

// timeouts for health reporting
#define BARO_TIMEOUT_MS                 500     // timeout in ms since last successful read
#define BARO_DATA_CHANGE_TIMEOUT_MS     2000    // timeout in ms since last successful read that involved temperature of pressure changing

class AP_Baro_Backend;

class AP_Baro
{
    friend class AP_Baro_Backend;
    friend class AP_Baro_SITL; // for access to sensors[]
    friend class AP_Baro_DroneCAN; // for access to sensors[]

public:
    AP_Baro();

    /* Do not allow copies */
    CLASS_NO_COPY(AP_Baro);

    // get singleton
    static AP_Baro *get_singleton(void) {
        return _singleton;
    }

    // barometer types
    typedef enum {
        BARO_TYPE_AIR,
        BARO_TYPE_WATER
    } baro_type_t;

    // initialise the barometer object, loading backend drivers
    __INITFUNC__ void init(void);

    // update the barometer object, asking backends to push data to
    // the frontend
    void update(void);

    // healthy - returns true if sensor and derived altitude are good
    bool healthy(void) const { return healthy(_primary); }

    bool healthy(uint8_t instance) const;

    // check if all baros are healthy - used for SYS_STATUS report
    bool all_healthy(void) const;

    // returns false if we fail arming checks, in which case the buffer will be populated with a failure message
    bool arming_checks(size_t buflen, char *buffer) const;

    // get primary sensor
    uint8_t get_primary(void) const { return _primary; }

    // pressure in Pascal. Divide by 100 for millibars or hectopascals
    float get_pressure(void) const { return get_pressure(_primary); }
    float get_pressure(uint8_t instance) const { return sensors[instance].pressure; }
#if HAL_BARO_WIND_COMP_ENABLED
    // dynamic pressure in Pascal. Divide by 100 for millibars or hectopascals
    const Vector3f& get_dynamic_pressure(uint8_t instance) const { return sensors[instance].dynamic_pressure; }
#endif
#if (HAL_BARO_WIND_COMP_ENABLED || AP_BARO_THST_COMP_ENABLED)
    float get_corrected_pressure(uint8_t instance) const { return sensors[instance].corrected_pressure; }
#endif

    // temperature in degrees C
    float get_temperature(void) const { return get_temperature(_primary); }
    float get_temperature(uint8_t instance) const { return sensors[instance].temperature; }

    // get pressure correction in Pascal. Divide by 100 for millibars or hectopascals
    float get_pressure_correction(void) const { return get_pressure_correction(_primary); }
    float get_pressure_correction(uint8_t instance) const { return sensors[instance].p_correction; }

    // calibrate the barometer. This must be called on startup if the
    // altitude/climb_rate/acceleration interfaces are ever used
    void calibrate(bool save=true);

    // update the barometer calibration to the current pressure. Can
    // be used for incremental preflight update of baro
    void update_calibration(void);

    // get current altitude in meters relative to altitude at the time
    // of the last calibrate() call
    float get_altitude(void) const { return get_altitude(_primary); }
    float get_altitude(uint8_t instance) const { return sensors[instance].altitude; }

    // get altitude above mean sea level
    float get_altitude_AMSL(uint8_t instance) const { return get_altitude(instance) + _field_elevation_active; }
    float get_altitude_AMSL(void) const { return get_altitude_AMSL(_primary); }

    // returns which i2c bus is considered "the" external bus
    uint8_t external_bus() const { return _ext_bus; }

    // Atmospheric Model Functions
    static float geometric_alt_to_geopotential(float alt);
    static float geopotential_alt_to_geometric(float alt);

    float get_temperature_from_altitude(float alt) const;
    float get_altitude_from_pressure(float pressure) const;

    // EAS2TAS for SITL
    static float get_EAS2TAS_for_alt_amsl(float alt_amsl);

#if AP_BARO_1976_STANDARD_ATMOSPHERE_ENABLED
    // lookup expected pressure for a given altitude. Used for SITL backend
    static void get_pressure_temperature_for_alt_amsl(float alt_amsl, float &pressure, float &temperature_K);
#endif

    // lookup expected temperature in degrees C for a given altitude. Used for SITL backend
    static float get_temperatureC_for_alt_amsl(const float alt_amsl);

    // lookup expected pressure in Pa for a given altitude. Used for SITL backend
    static float get_pressure_for_alt_amsl(const float alt_amsl);
    
    // get air density for SITL
    static float get_air_density_for_alt_amsl(float alt_amsl);

    // get altitude difference in meters relative given a base
    // pressure in Pascal
    float get_altitude_difference(float base_pressure, float pressure) const;

    // get sea level pressure relative to 1976 standard atmosphere model
    // pressure in Pascal
    float get_sealevel_pressure(float pressure, float altitude) const;

    // get scale factor required to convert equivalent to true
    // airspeed. This should only be used to update the AHRS value
    // once per loop. Please use AP::ahrs().get_EAS2TAS()
    float _get_EAS2TAS(void) const;

    // get air density / sea level density - decreases as altitude climbs
    // please use AP::ahrs()::get_air_density_ratio()
    float _get_air_density_ratio(void);

    // get current climb rate in meters/s. A positive number means
    // going up
    float get_climb_rate(void);

    // ground temperature in degrees C
    // the ground values are only valid after calibration
    float get_ground_temperature(void) const;

    // ground pressure in Pascal
    // the ground values are only valid after calibration
    float get_ground_pressure(void) const { return get_ground_pressure(_primary); }
    float get_ground_pressure(uint8_t i)  const { return sensors[i].ground_pressure.get(); }

    // set the temperature to be used for altitude calibration. This
    // allows an external temperature source (such as a digital
    // airspeed sensor) to be used as the temperature source
    void set_external_temperature(float temperature);

    // get last time sample was taken (in ms)
    uint32_t get_last_update(void) const { return get_last_update(_primary); }
    uint32_t get_last_update(uint8_t instance) const { return sensors[instance].last_update_ms; }

    // settable parameters
    static const struct AP_Param::GroupInfo var_info[];

    float get_external_temperature(void) const { return get_external_temperature(_primary); };
    float get_external_temperature(const uint8_t instance) const;

    // Set the primary baro
    void set_primary_baro(uint8_t primary) { _primary_baro.set_and_save(primary); };

    // Set the type (Air or Water) of a particular instance
    void set_type(uint8_t instance, baro_type_t type) { sensors[instance].type = type; };

    // Get the type (Air or Water) of a particular instance
    baro_type_t get_type(uint8_t instance) { return sensors[instance].type; };

    // register a new sensor, claiming a sensor slot. If we are out of
    // slots it will panic
    uint8_t register_sensor(void);

    // return number of registered sensors
    uint8_t num_instances(void) const { return _num_sensors; }

    // set baro drift amount
    void set_baro_drift_altitude(float alt) { _alt_offset.set(alt); }

    // get baro drift amount
    float get_baro_drift_offset(void) const { return _alt_offset_active; }

    // simple underwater atmospheric model
    static void SimpleUnderWaterAtmosphere(float alt, float &rho, float &delta, float &theta);

    // set a pressure correction from AP_TempCalibration
    void set_pressure_correction(uint8_t instance, float p_correction);

    uint8_t get_filter_range() const { return _filter_range; }

    // indicate which bit in LOG_BITMASK indicates baro logging enabled
    void set_log_baro_bit(uint32_t bit) { _log_baro_bit = bit; }
    bool should_log() const;

    // allow threads to lock against baro update
    HAL_Semaphore &get_semaphore(void) {
        return _rsem;
    }

#if AP_BARO_MSP_ENABLED
    void handle_msp(const MSP::msp_baro_data_message_t &pkt);
#endif
#if AP_BARO_EXTERNALAHRS_ENABLED
    void handle_external(const AP_ExternalAHRS::baro_data_message_t &pkt);
#endif

    enum Options : uint16_t {
        TreatMS5611AsMS5607     = (1U << 0U),
    };

    // check if an option is set
    bool option_enabled(const Options option) const
    {
        return (uint16_t(_options.get()) & uint16_t(option)) != 0;
    }

private:
    // singleton
    static AP_Baro *_singleton;
    
    // how many drivers do we have?
    uint8_t _num_drivers;
    AP_Baro_Backend *drivers[BARO_MAX_DRIVERS];

    // how many sensors do we have?
    uint8_t _num_sensors;

    // what is the primary sensor at the moment?
    uint8_t _primary;

    uint32_t _log_baro_bit = -1;

    bool init_done;

    uint8_t msp_instance_mask;

    // bitmask values for GND_PROBE_EXT
    enum {
        PROBE_BMP085=(1<<0),
        PROBE_BMP280=(1<<1),
        PROBE_MS5611=(1<<2),
        PROBE_MS5607=(1<<3),
        PROBE_MS5637=(1<<4),
        PROBE_FBM320=(1<<5),
        PROBE_DPS280=(1<<6),
        PROBE_LPS25H=(1<<7),
        PROBE_KELLER=(1<<8),
        PROBE_MS5837=(1<<9),
        PROBE_BMP388=(1<<10),
        PROBE_SPL06 =(1<<11),
        PROBE_MSP   =(1<<12),
        PROBE_BMP581=(1<<13),
        PROBE_AUAV  =(1<<14),
    };
    
#if HAL_BARO_WIND_COMP_ENABLED
    class WindCoeff {
    public:
        static const struct AP_Param::GroupInfo var_info[];

        AP_Int8  enable; // enable compensation for this barometer
        AP_Float xp;     // ratio of static pressure rise to dynamic pressure when flying forwards
        AP_Float xn;     // ratio of static pressure rise to dynamic pressure when flying backwards
        AP_Float yp;     // ratio of static pressure rise to dynamic pressure when flying to the right
        AP_Float yn;     // ratio of static pressure rise to dynamic pressure when flying to the left
        AP_Float zp;     // ratio of static pressure rise to dynamic pressure when flying up
        AP_Float zn;     // ratio of static pressure rise to dynamic pressure when flying down
    };
#endif

    struct sensor {
        uint32_t last_update_ms;        // last update time in ms
        uint32_t last_change_ms;        // last update time in ms that included a change in reading from previous readings
        float pressure;                 // pressure in Pascal
        float temperature;              // temperature in degrees C
        float altitude;                 // calculated altitude
        AP_Float ground_pressure;
        float p_correction;
        baro_type_t type;               // 0 for air pressure (default), 1 for water pressure
        bool healthy;                   // true if sensor is healthy
        bool alt_ok;                    // true if calculated altitude is ok
        bool calibrated;                // true if calculated calibrated successfully
        AP_Int32 bus_id;
#if HAL_BARO_WIND_COMP_ENABLED
        WindCoeff wind_coeff;
        Vector3f dynamic_pressure;      // calculated dynamic pressure
#endif
#if AP_BARO_THST_COMP_ENABLED
        AP_Float mot_scale;             // thrust-based pressure scaling
#endif
#if (HAL_BARO_WIND_COMP_ENABLED || AP_BARO_THST_COMP_ENABLED)
        float corrected_pressure;
#endif
    } sensors[BARO_MAX_INSTANCES];

    AP_Float                            _alt_offset;
    float                               _alt_offset_active;
    AP_Float                            _field_elevation;       // field elevation in meters
    float                               _field_elevation_active;
    uint32_t                            _field_elevation_last_ms;
    AP_Int8                             _primary_baro; // primary chosen by user
    AP_Int8                             _ext_bus; // bus number for external barometer
    float                               _external_temperature;
    uint32_t                            _last_external_temperature_ms;
    DerivativeFilterFloat_Size7         _climb_rate_filter;
    AP_Float                            _specific_gravity; // the specific gravity of fluid for an ROV 1.00 for freshwater, 1.024 for salt water
    AP_Float                            _user_ground_temperature; // user override of the ground temperature used for EAS2TAS
    float                               _guessed_ground_temperature; // currently ground temperature estimate using our best available source

    // when did we last notify the GCS of new pressure reference?
    uint32_t                            _last_notify_ms;

    // see if we already have probed a i2c driver by bus number and address
    bool _have_i2c_driver(uint8_t bus_num, uint8_t address) const;
    bool _add_backend(AP_Baro_Backend *backend);
    void _probe_i2c_barometers(void);
    AP_Int8                            _filter_range;  // valid value range from mean value
    AP_Int32                           _baro_probe_ext;

#ifndef HAL_BUILD_AP_PERIPH
    AP_Float                           _alt_error_max;
#endif

    AP_Int16                           _options;

    // semaphore for API access from threads
    HAL_Semaphore                      _rsem;

#if HAL_BARO_WIND_COMP_ENABLED
    /*
      return pressure correction for wind based on GND_WCOEF parameters
    */
    float wind_pressure_correction(uint8_t instance);
#endif
#if AP_BARO_THST_COMP_ENABLED
    float thrust_pressure_correction(uint8_t instance);
#endif
    // Logging function
    void Write_Baro(void);
    void Write_Baro_instance(uint64_t time_us, uint8_t baro_instance);

    void update_field_elevation();

    // atmosphere model functions
    float get_altitude_difference_extended(float base_pressure, float pressure) const;
    float get_EAS2TAS_extended(float pressure) const;
    static float get_temperature_by_altitude_layer(float alt, int8_t idx);

    float get_altitude_difference_simple(float base_pressure, float pressure) const;
    float get_EAS2TAS_simple(float altitude, float pressure) const;
};

namespace AP {
    AP_Baro &baro();
};
