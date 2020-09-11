//--------------------------------------------------------------------------------------------------
/**
 * @file avPublisher.c
 *
 * Implements a connection between AirVantage and a mangOH Yellow.
 *
 * "Variables" are provided to AirVantage that allow it to read, on-demand, sensors' current value.
 *
 * This AirVantage publisher is using the same mechanism as the one in RedSensorToCloud application.
 *
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "json.h"


//--------------------------------------------------------------------------------------------------
/*
 * Polling, filtering and buffering configuration defaults.
 */
//--------------------------------------------------------------------------------------------------

// Polling periods (seconds):
#define ACCEL_PERIOD 10
#define GYRO_PERIOD 10
#define TEMP_PERIOD 10
#define POS_PERIOD 10
#define SIGQUAL_PERIOD 10

// Buffer sizes (# of samples):
#define ACCEL_BUFFER_COUNT 100
#define GYRO_BUFFER_COUNT 100
#define TEMP_BUFFER_COUNT 100
#define POS_BUFFER_COUNT 100
#define SIGQUAL_BUFFER_COUNT 100

// Change-by thresholds:
#define TEMP_CHANGE_BY 2.0  // degC

// Data Hub Observation resource paths:
#define ACCEL_OBS_PATH "/obs/accel"
#define GYRO_OBS_PATH "/obs/gyro"
#define TEMP_OBS_PATH "/obs/temperature"
#define POS_OBS_PATH "/obs/position"
#define SIGQUAL_OBS_PATH "/obs/signalQuality"

// Data Hub sensor Input resource paths:
#define ACCEL_SENSOR_INPUT_PATH     "/app/yellowSensor/accel/value"
#define GYRO_SENSOR_INPUT_PATH      "/app/yellowSensor/gyro/value"
#define POS_SENSOR_INPUT_PATH       "/app/yellowSensor/position/value"
#define TEMP_SENSOR_INPUT_PATH      "/app/yellowSensor/temp/value"
#define SIGQUAL_SENSOR_INPUT_PATH   "/app/yellowSensor/signalQuality/value"

//--------------------------------------------------------------------------------------------------
/*
 * type definitions
 */
//--------------------------------------------------------------------------------------------------

/// Structure that holds variables needed to manage one sensor's data.
typedef struct
{
    const char* obsPath; ///< String containing Data Hub observation path to fetch data from.
    double lastDeliveredTimestamp; ///< Timestamp of newest sample successfully delivered to cloud.
    double timestamp; ///< Timestamp of sample we are trying to push to the cloud.
    enum
    {
        SENSOR_STATE_IDLE,      ///< No data to send.
        SENSOR_STATE_PUSHING,   ///< Sending data to the cloud.
        SENSOR_STATE_BACKLOGGED,///< Sending data to the cloud and more data waiting to be sent.
        SENSOR_STATE_FAULT,     ///< Failed to push data.

    } state; ///< State of the sensor.
}
Sensor_t;


//--------------------------------------------------------------------------------------------------
/*
 * variable definitions
 */
//--------------------------------------------------------------------------------------------------

/// True if the AirVantage session is active.  False if not.
static bool IsAvSessionActive = false;


/// Cloud push tracking record for the accelerometer.
static Sensor_t Accelerometer = {
    .obsPath=ACCEL_OBS_PATH,
    .lastDeliveredTimestamp=0,
    .timestamp=0,
    .state=SENSOR_STATE_IDLE,
};

/// Cloud push tracking record for the gyroscope.
static Sensor_t Gyroscope = {
    .obsPath=GYRO_OBS_PATH,
    .lastDeliveredTimestamp=0,
    .timestamp=0,
    .state=SENSOR_STATE_IDLE,
};

/// Cloud push tracking record for the temperature.
static Sensor_t Thermometer = {
    .obsPath=TEMP_OBS_PATH,
    .lastDeliveredTimestamp=0,
    .timestamp=0,
    .state=SENSOR_STATE_IDLE,
};

/// Cloud push tracking record for the position.
static Sensor_t PositionSensor = {
    .obsPath=POS_OBS_PATH,
    .lastDeliveredTimestamp=0,
    .timestamp=0,
   .state=SENSOR_STATE_IDLE,
};

/// Cloud push tracking record for the signal quality.
static Sensor_t SignalQuality = {
    .obsPath=SIGQUAL_OBS_PATH,
    .lastDeliveredTimestamp=0,
    .timestamp=0,
    .state=SENSOR_STATE_IDLE,
};


//--------------------------------------------------------------------------------------------------
/*
 * static function definitions
 */
//--------------------------------------------------------------------------------------------------

static void PushBacklog(Sensor_t* sensorPtr);


//--------------------------------------------------------------------------------------------------
/**
 * Handles notification of AirVantage time-series push status.
 */
//--------------------------------------------------------------------------------------------------
static void HandleAvPushComplete
(
    le_avdata_PushStatus_t status, ///< Push success/failure status
    void* context                  ///< Not used
)
{
    Sensor_t* sensorPtr = context;

    switch (status)
    {
        case LE_AVDATA_PUSH_SUCCESS:
            // Remember the timestamp we last successfully delivered.
            sensorPtr->lastDeliveredTimestamp = sensorPtr->timestamp;

            // If there's more data to push, push it now.
            if (sensorPtr->state == SENSOR_STATE_BACKLOGGED)
            {
                PushBacklog(sensorPtr);
            }

            return;

        case LE_AVDATA_PUSH_FAILED:
            LE_WARN("Push to AirVantage failed (%s). Retrying...", sensorPtr->obsPath);

            // Try this one again.
            PushBacklog(sensorPtr);

            return;
    }

    LE_FATAL("Unexpected push result status %d (%s).", status, sensorPtr->obsPath);
}


//--------------------------------------------------------------------------------------------------
/**
 * Records a temperature reading into an avdata record and pushes it.
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the record is full
 *      - LE_FAULT non-specific failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t PushTemperature
(
    double timestamp,
    double value
)
{
    // Convert the timestamp to an integer number of milliseconds.
    uint64_t ms = (uint64_t)(timestamp * 1000.0);

    le_avdata_RecordRef_t rec = le_avdata_CreateRecord();

    const char *path = "MangOH.Sensors.Pressure.Temperature";

    le_result_t result = le_avdata_RecordFloat(rec, path, value, ms);
    if (result != LE_OK)
    {
        LE_ERROR("Couldn't record pressure sensor reading - %s", LE_RESULT_TXT(result));
        goto done;
    }

    result = le_avdata_PushRecord(rec, HandleAvPushComplete, &Thermometer);
    if ((result != LE_OK) && (result != LE_BUSY))
    {
        LE_CRIT("Failed to push to AirVantage Agent (%s).", LE_RESULT_TXT(result));
        goto done;
    }

done:
    le_avdata_DeleteRecord(rec);

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Records a signal quality reading into an avdata record and pushes it.
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the record is full
 *      - LE_FAULT non-specific failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t PushSignalQuality
(
    double timestamp,
    double value
)
{
    // Convert the timestamp to an integer number of milliseconds.
    uint64_t ms = (uint64_t)(timestamp * 1000.0);

    le_avdata_RecordRef_t rec = le_avdata_CreateRecord();

    const char *path = "MangOH.Sensors.SignalQuality";

    le_result_t result = le_avdata_RecordFloat(rec, path, value, ms);
    if (result != LE_OK)
    {
        LE_ERROR("Couldn't record signal quality reading - %s", LE_RESULT_TXT(result));
        goto done;
    }

    result = le_avdata_PushRecord(rec, HandleAvPushComplete, &SignalQuality);
    if ((result != LE_OK) && (result != LE_BUSY))
    {
        LE_CRIT("Failed to push to AirVantage Agent (%s).", LE_RESULT_TXT(result));
        goto done;
    }

done:
    le_avdata_DeleteRecord(rec);

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Extract a numerical member from a JSON structure.
 *
 * @return The number, or NAN if failed (check using isnan()).
 */
//--------------------------------------------------------------------------------------------------
static double ExtractNumber
(
    const char* json,
    const char* memberName
)
{
    char member[32];
    json_DataType_t dataType;

    le_result_t result = json_Extract(member, sizeof(member), json, memberName, &dataType);

    if (result != LE_OK)
    {
        LE_ERROR("'%s' not found in JSON value '%s'.", memberName, json);
        return NAN;
    }

    if (dataType != JSON_TYPE_NUMBER)
    {
        LE_ERROR("'%s' has wrong data type (%s) in JSON value '%s'.",
                 memberName,
                 json_GetDataTypeName(dataType),
                 json);
        return NAN;
    }

    double number = json_ConvertToNumber(member);
    if (isnan(number))
    {
        LE_CRIT("Unable to convert '%s' to a number! (member '%s' of '%s')",
                member,
                memberName,
                json);
    }

    return number;
}

//--------------------------------------------------------------------------------------------------
/**
 * Records an accelerometer reading into an avdata record and pushes it.
 *
 * The JSON value is expected to look like this:
 *
 * {"x":-1.094340, "y":0.085514, "z":9.778496}
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the record is full
 *      - LE_FAULT non-specific failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t PushAcceleration
(
    double timestamp,
    const char* value   ///< JSON string.
)
{
    double x = ExtractNumber(value, "x");
    if (isnan(x))
    {
        LE_ERROR("Failed to decode accelerometer value.");
        return LE_FAULT;
    }

    double y = ExtractNumber(value, "y");
    if (isnan(y))
    {
        LE_ERROR("Failed to decode accelerometer value.");
        return LE_FAULT;
    }

    double z = ExtractNumber(value, "z");
    if (isnan(z))
    {
        LE_ERROR("Failed to decode accelerometer value.");
        return LE_FAULT;
    }

    // Convert the timestamp to an integer number of milliseconds.
    uint64_t ms = (uint64_t)(timestamp * 1000.0);

    le_avdata_RecordRef_t rec = le_avdata_CreateRecord();

    // The '_' is a placeholder that will be replaced
    static char path[] = "MangOH.Sensors.Accelerometer.Acceleration._";

    le_result_t result;

    path[sizeof(path) - 2] = 'X';
    result = le_avdata_RecordFloat(rec, path, x, ms);
    if (result != LE_OK)
    {
        LE_ERROR("Couldn't record accelerometer x reading - %s", LE_RESULT_TXT(result));
        goto done;
    }

    path[sizeof(path) - 2] = 'Y';
    result = le_avdata_RecordFloat(rec, path, y, ms);
    if (result != LE_OK)
    {
        LE_ERROR("Couldn't record accelerometer y reading - %s", LE_RESULT_TXT(result));
        goto done;
    }

    path[sizeof(path) - 2] = 'Z';
    result = le_avdata_RecordFloat(rec, path, z, ms);
    if (result != LE_OK)
    {
        LE_ERROR("Couldn't record accelerometer z reading - %s", LE_RESULT_TXT(result));
        goto done;
    }

    result = le_avdata_PushRecord(rec, HandleAvPushComplete, &Accelerometer);
    if ((result != LE_OK) && (result != LE_BUSY))
    {
        LE_CRIT("Failed to push to AirVantage Agent (%s).", LE_RESULT_TXT(result));
        goto done;
    }

done:
    le_avdata_DeleteRecord(rec);

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Records an angular velocity (gyro) reading into an avdata record and pushes it.
 *
 * The JSON value is expected to look like this:
 *
 * {"x":-0.008520, "y":-0.006390, "z":-0.007455}
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the record is full
 *      - LE_FAULT non-specific failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t PushAngularVelocity
(
    double timestamp,
    const char* value   ///< JSON string.
)
{
    double x = ExtractNumber(value, "x");
    if (isnan(x))
    {
        LE_ERROR("Failed to decode gyro value.");
        return LE_FAULT;
    }

    double y = ExtractNumber(value, "y");
    if (isnan(y))
    {
        LE_ERROR("Failed to decode gyro value.");
        return LE_FAULT;
    }

    double z = ExtractNumber(value, "z");
    if (isnan(z))
    {
        LE_ERROR("Failed to decode gyro value.");
        return LE_FAULT;
    }

    // Convert the timestamp to an integer number of milliseconds.
    uint64_t ms = (uint64_t)(timestamp * 1000.0);

    le_avdata_RecordRef_t rec = le_avdata_CreateRecord();

    // The '_' is a placeholder that will be replaced
    char path[] = "MangOH.Sensors.Accelerometer.Gyro._";
    le_result_t result;

    path[sizeof(path) - 2] = 'X';
    result = le_avdata_RecordFloat(rec, path, x, ms);
    if (result != LE_OK)
    {
        LE_ERROR("Couldn't record gyro x reading - %s", LE_RESULT_TXT(result));
        goto done;
    }

    path[sizeof(path) - 2] = 'Y';
    result = le_avdata_RecordFloat(rec, path, y, ms);
    if (result != LE_OK)
    {
        LE_ERROR("Couldn't record gyro y reading - %s", LE_RESULT_TXT(result));
        goto done;
    }

    path[sizeof(path) - 2] = 'Z';
    result = le_avdata_RecordFloat(rec, path, z, ms);
    if (result != LE_OK)
    {
        LE_ERROR("Couldn't record gyro z reading - %s", LE_RESULT_TXT(result));
        goto done;
    }

    result = le_avdata_PushRecord(rec, HandleAvPushComplete, &Gyroscope);
    if ((result != LE_OK) && (result != LE_BUSY))
    {
        LE_CRIT("Failed to push to AirVantage Agent (%s).", LE_RESULT_TXT(result));
        goto done;
    }

done:
    le_avdata_DeleteRecord(rec);

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Records a position reading into a given avdata record.
 *
 * The JSON value is expected to look like this:
 *
 * { "lat": 49.172350, "lon": -123.070987, "hAcc": 14.000000, "alt": 0.009000, "vAcc": 8.000000 }
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the record is full
 *      - LE_FAULT non-specific failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t PushPosition
(
    double timestamp,
    const char* value   ///< JSON string.
)
{
    double latitude = ExtractNumber(value, "lat");
    if (isnan(latitude))
    {
        LE_ERROR("Failed to decode position value.");
        return LE_FAULT;
    }

    double longitude = ExtractNumber(value, "lon");
    if (isnan(longitude))
    {
        LE_ERROR("Failed to decode position value.");
        return LE_FAULT;
    }

    double hAccuracy = ExtractNumber(value, "hAcc");
    if (isnan(hAccuracy))
    {
        LE_ERROR("Failed to decode position value.");
        return LE_FAULT;
    }

    double altitude = ExtractNumber(value, "alt");
    if (isnan(altitude))
    {
        LE_ERROR("Failed to decode position value.");
        return LE_FAULT;
    }

    double vAccuracy = ExtractNumber(value, "vAcc");
    if (isnan(vAccuracy))
    {
        LE_ERROR("Failed to decode position value.");
        return LE_FAULT;
    }

    // Convert the timestamp to an integer number of milliseconds.
    uint64_t ms = (uint64_t)(timestamp * 1000.0);

    le_avdata_RecordRef_t rec = le_avdata_CreateRecord();

    // The '_' is a placeholder that will be replaced
    char path[] = "lwm2m.6.0._";
    le_result_t result;

    path[sizeof(path) - 2] = '0';
    result = le_avdata_RecordFloat(rec, "MangOH.Sensors.Gps.Latitude", latitude, ms);
    if (result != LE_OK)
    {
        LE_ERROR("Couldn't record gps latitude reading - %s", LE_RESULT_TXT(result));
        goto done;
    }

    path[sizeof(path) - 2] = '1';
    result = le_avdata_RecordFloat(rec, "MangOH.Sensors.Gps.Longitude", longitude, ms);
    if (result != LE_OK)
    {
        LE_ERROR("Couldn't record gps longitude reading - %s", LE_RESULT_TXT(result));
        goto done;
    }

    path[sizeof(path) - 2] = '3';
    result = le_avdata_RecordFloat(rec, "MangOH.Sensors.Gps.HorizontalAccuracy", hAccuracy, ms);
    if (result != LE_OK)
    {
        LE_ERROR("Couldn't record gps horizontal accuracy reading - %s", LE_RESULT_TXT(result));
        goto done;
    }

    path[sizeof(path) - 2] = '2';
    result = le_avdata_RecordFloat(rec, "MangOH.Sensors.Gps.Altitude", altitude, ms);
    if (result != LE_OK)
    {
        LE_ERROR("Couldn't record gps altitude reading - %s", LE_RESULT_TXT(result));
        goto done;
    }

    result = le_avdata_RecordFloat(rec, "MangOH.Sensors.Gps.VerticalAccuracy", vAccuracy, ms);
    if (result != LE_OK)
    {
        LE_ERROR("Couldn't record gps vertical accuracy reading - %s", LE_RESULT_TXT(result));
        goto done;
    }

    result = le_avdata_PushRecord(rec, HandleAvPushComplete, &PositionSensor);
    if ((result != LE_OK) && (result != LE_BUSY))
    {
        LE_CRIT("Failed to push to AirVantage Agent (%s).", LE_RESULT_TXT(result));
        goto done;
    }

done:
    le_avdata_DeleteRecord(rec);

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Push a numeric sensor sample to the cloud.
 */
//--------------------------------------------------------------------------------------------------
static void PushNumeric
(
    Sensor_t* sensorPtr,
    double timestamp,
    double value
)
{
    le_result_t result;

    sensorPtr->timestamp = timestamp;
    if (sensorPtr == &Thermometer)
    {
        result = PushTemperature(timestamp, value);
    }
    else if (sensorPtr == &SignalQuality)
    {
        result = PushSignalQuality(timestamp, value);
    }
    else
    {
        LE_FATAL("Unexpected numeric sensor '%s'.", sensorPtr->obsPath);
    }

    if (result != LE_OK)
    {
        LE_CRIT("Failed (%s) delivery of '%s' stalled.", LE_RESULT_TXT(result), sensorPtr->obsPath);

        sensorPtr->state = SENSOR_STATE_FAULT;

        // Wait for another update from the sensor to trigger a retry.
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Push a JSON sensor sample to the cloud.
 */
//--------------------------------------------------------------------------------------------------
static void PushJson
(
    Sensor_t* sensorPtr,
    double timestamp,
    const char* value
)
{
    le_result_t result;

    sensorPtr->timestamp = timestamp;

    if (sensorPtr == &Accelerometer)
    {
        result = PushAcceleration(timestamp, value);
    }
    else if (sensorPtr == &Gyroscope)
    {
        result = PushAngularVelocity(timestamp, value);
    }
    else if (sensorPtr == &PositionSensor)
    {
       result = PushPosition(timestamp, value);
    }
    else
    {
        LE_FATAL("Unexpected numeric sensor '%s'.", sensorPtr->obsPath);
    }

    if (result == LE_FORMAT_ERROR)
    {
        LE_CRIT("Discarding malformed value from '%s' (%s).", sensorPtr->obsPath, value);

        sensorPtr->lastDeliveredTimestamp = timestamp;
        if (sensorPtr->state == SENSOR_STATE_PUSHING)
        {
            sensorPtr->state = SENSOR_STATE_IDLE;
        }
    }

    if (result != LE_OK)
    {
        LE_CRIT("%s: Delivery of '%s' stalled.", LE_RESULT_TXT(result), sensorPtr->obsPath);

        sensorPtr->state = SENSOR_STATE_FAULT;

        // Wait for another update from the sensor to trigger a retry.
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Push a sensor's backlog (or at least, the oldest samples of the backlog).
 */
//--------------------------------------------------------------------------------------------------
static void PushBacklog
(
    Sensor_t* sensorPtr
)
{
    // Fetch the oldest undelivered record from the Data Hub observation buffer for this sensor.
    if (   (sensorPtr == &Accelerometer)
        || (sensorPtr == &Gyroscope) 
        || (sensorPtr == &PositionSensor)  )
    {
        double timestamp;
        char value[IO_MAX_STRING_VALUE_LEN + 1];

        le_result_t result = dhubQuery_ReadBufferSampleJson(sensorPtr->obsPath,
                                                            sensorPtr->lastDeliveredTimestamp,
                                                            &timestamp,
                                                            value,
                                                            sizeof(value));
        if (result == LE_OK)
        {
            PushJson(sensorPtr, timestamp, value);
        }
        else if (result == LE_NOT_FOUND)
        {
            sensorPtr->state = SENSOR_STATE_IDLE;
        }
        else
        {
            LE_CRIT("Unexpected result code (%s) from Data Hub query.", LE_RESULT_TXT(result));
        }
    }
    else if (  (sensorPtr == &Thermometer) 
               /* || (sensorPtr == &SignalQuality) */  )
    {
        double timestamp;
        double value;

        le_result_t result = dhubQuery_ReadBufferSampleNumeric(sensorPtr->obsPath,
                                                               sensorPtr->lastDeliveredTimestamp,
                                                               &timestamp,
                                                               &value);
        if (result == LE_OK)
        {
            PushNumeric(sensorPtr, timestamp, value);
        }
        else if (result == LE_NOT_FOUND)
        {
            sensorPtr->state = SENSOR_STATE_IDLE;
        }
        else
        {
            LE_CRIT("Unexpected result code (%s) from Data Hub query.", LE_RESULT_TXT(result));
        }
    }
    else
    {
        LE_FATAL("Unrecognized sensor object %p.", sensorPtr);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Handle changes in the AirVantage session state
 *
 * Actually, we don't really need to do anything here, because the AV Agent will queue our
 * push requests until the session comes back anyway.
 */
//--------------------------------------------------------------------------------------------------
static void AvSessionStateHandler
(
    le_avdata_SessionState_t state,
    void *context   ///< Not used.
)
{
    switch (state)
    {
        case LE_AVDATA_SESSION_STARTED:
            // Temporary workaround for the session state problem.
            if (IsAvSessionActive)
            {
                LE_ERROR("Received 'session started' indication when already started.");
            }
            else
            {
                LE_INFO("AirVantage(tm) session started");
                IsAvSessionActive = true;
            }
            break;

        case LE_AVDATA_SESSION_STOPPED:
            LE_INFO("AirVantage(tm) session stopped");
            IsAvSessionActive = false;
            break;

        default:
            LE_ERROR("Unsupported AV session state %d", state);
            break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Call-back function that gets called when a numeric sensor update is received from the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void HandleNumericUpdate
(
    double timestamp,
    double value,
    void* contextPtr    ///< Pointer to the Sensor_t object associated with the sensor.
)
{
    Sensor_t* sensorPtr = contextPtr;

    switch (sensorPtr->state)
    {
        case SENSOR_STATE_IDLE:
            sensorPtr->state = SENSOR_STATE_PUSHING;
            PushNumeric(sensorPtr, timestamp, value);
            break;

        case SENSOR_STATE_PUSHING:
            sensorPtr->state = SENSOR_STATE_BACKLOGGED;
            break;

        case SENSOR_STATE_BACKLOGGED:
            // Don't need to do anything.  Completion of the current push will result in more
            // being pushed.
            break;

        case SENSOR_STATE_FAULT:
            sensorPtr->state = SENSOR_STATE_BACKLOGGED;
            PushBacklog(sensorPtr);
            break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Call-back function that gets called when a JSON sensor update is received from the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void HandleJsonUpdate
(
    double timestamp,
    const char* value,
    void* contextPtr    ///< Pointer to the Sensor_t object associated with the sensor.
)
{
    Sensor_t* sensorPtr = contextPtr;

    switch (sensorPtr->state)
    {
        case SENSOR_STATE_IDLE:
            sensorPtr->state = SENSOR_STATE_PUSHING;
            PushJson(sensorPtr, timestamp, value);
            break;

        case SENSOR_STATE_PUSHING:
            sensorPtr->state = SENSOR_STATE_BACKLOGGED;
            break;

        case SENSOR_STATE_BACKLOGGED:
            // Don't need to do anything.  Completion of the current push will result in more
            // being pushed.
            break;

        case SENSOR_STATE_FAULT:
            sensorPtr->state = SENSOR_STATE_BACKLOGGED;
            PushBacklog(sensorPtr);
            break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Create an Obsevation with a buffer in the data hub.
 */
//--------------------------------------------------------------------------------------------------
static void CreateObservation
(
    Sensor_t* sensorPtr,
    unsigned int bufferMaxCount,
    double changeBy ///< Ignored if 0
)
{
    le_result_t result = dhubAdmin_CreateObs(sensorPtr->obsPath);
    if (result != LE_OK)
    {
        LE_FATAL("Failed to create Data Hub observation at path '%s' (%s).",
                 sensorPtr->obsPath,
                 LE_RESULT_TXT(result));
    }

    dhubAdmin_SetBufferMaxCount(sensorPtr->obsPath, bufferMaxCount);

    if (changeBy != 0.0)
    {
        dhubAdmin_SetChangeBy(sensorPtr->obsPath, changeBy);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Configure and enable a sensor whose 'value' input is at a given path.
 */
//--------------------------------------------------------------------------------------------------
static void ConfigureSensor
(
    const char* inputPath,
    double period ///< seconds
)
{
    const char* lastSlashPtr = strrchr(inputPath, '/');
    if (lastSlashPtr == NULL)
    {
        LE_FATAL("No '/' found in path '%s'.", inputPath);
    }

    char path[DHUBIO_MAX_RESOURCE_PATH_LEN + 1];

    size_t basePathLen = (lastSlashPtr - inputPath) + 1; // +1 to include the slash.

    // Buffer size check.
    LE_ASSERT((basePathLen + sizeof("period")) < DHUBIO_MAX_RESOURCE_PATH_LEN);

    // Set the period.
    (void)strncpy(path, inputPath, basePathLen);    // WARNING: May not be null-terminated.
    (void)strcpy(path + basePathLen, "period");   // Guaranteed to null-terminate.
    dhubAdmin_SetNumericDefault(path, period);

    // Enable the sensor.
    (void)strcpy(path + basePathLen, "enable");   // Guaranteed to null-terminate.
    dhubAdmin_PushBoolean(path, 0.0, true);
}

//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{

    // Create "observations" in the Data Hub for filtering, buffering, and receiving sensor updates.
    CreateObservation(&Accelerometer, ACCEL_BUFFER_COUNT, 0.0);
    CreateObservation(&Gyroscope, GYRO_BUFFER_COUNT, 0.0);
    CreateObservation(&PositionSensor, POS_BUFFER_COUNT, 0.0);
    CreateObservation(&Thermometer, TEMP_BUFFER_COUNT, TEMP_CHANGE_BY);
    CreateObservation(&SignalQuality, SIGQUAL_BUFFER_COUNT, 0.0);

    // Register for notification when the observations receive updates.
    dhubAdmin_AddJsonPushHandler(Accelerometer.obsPath, HandleJsonUpdate, &Accelerometer);
    dhubAdmin_AddJsonPushHandler(Gyroscope.obsPath, HandleJsonUpdate, &Gyroscope);
    dhubAdmin_AddJsonPushHandler(PositionSensor.obsPath, HandleJsonUpdate, &PositionSensor);
    dhubAdmin_AddNumericPushHandler(Thermometer.obsPath, HandleNumericUpdate, &Thermometer);
    dhubAdmin_AddNumericPushHandler(SignalQuality.obsPath, HandleNumericUpdate, &SignalQuality);

    // Configure the sensors.
    ConfigureSensor(ACCEL_SENSOR_INPUT_PATH, ACCEL_PERIOD);
    ConfigureSensor(GYRO_SENSOR_INPUT_PATH, GYRO_PERIOD);
    ConfigureSensor(POS_SENSOR_INPUT_PATH, POS_PERIOD);
    ConfigureSensor(TEMP_SENSOR_INPUT_PATH, TEMP_PERIOD);
    ConfigureSensor(SIGQUAL_SENSOR_INPUT_PATH, SIGQUAL_PERIOD);

    // Connect the observations to the sensor inputs in the Data Hub.
    dhubAdmin_SetSource(Accelerometer.obsPath, ACCEL_SENSOR_INPUT_PATH);
    dhubAdmin_SetSource(Gyroscope.obsPath, GYRO_SENSOR_INPUT_PATH);
    dhubAdmin_SetSource(PositionSensor.obsPath, POS_SENSOR_INPUT_PATH);
    dhubAdmin_SetSource(Thermometer.obsPath, TEMP_SENSOR_INPUT_PATH);
    dhubAdmin_SetSource(SignalQuality.obsPath, SIGQUAL_SENSOR_INPUT_PATH);

    // Request an AirVantage session.
    (void)le_avdata_AddSessionStateHandler(AvSessionStateHandler, NULL);
    LE_FATAL_IF(le_avdata_RequestSession() == NULL, "Failed to request avdata session");
}
