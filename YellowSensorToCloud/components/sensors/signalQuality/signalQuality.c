//--------------------------------------------------------------------------------------------------
/**
 *
 * Reads the signal quality value and sends it to the Legato Data Hub.
 *
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

#include "fileUtils.h"
#include "periodicSensor.h"

//--------------------------------------------------------------------------------------------------
/**
 * Read the signal quality.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t signalQuality_Read
(
    uint32_t* readingPtr
        ///< [OUT] Where the reading will be put if LE_OK is returned.
)
{

   le_result_t res;
   le_mrc_NetRegState_t  state;
   uint32_t              quality;
 
   res = le_mrc_GetNetRegState(&state);
   LE_ASSERT(res == LE_OK);
   LE_ASSERT((state>=LE_MRC_REG_NONE) && (state<=LE_MRC_REG_UNKNOWN));
 
   res = le_mrc_GetSignalQual(&quality);
   if (res != LE_OK)
    {
        LE_ERROR("Failed to read signal quality");
        goto done;
    }

    *readingPtr = quality;

done:
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Sample the signal quality and publish the results to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void SampleSignalQuality
(
    psensor_Ref_t ref, void *context
)
{
    uint32_t sample;

    le_result_t result = signalQuality_Read(&sample);

    if (result == LE_OK)
    {
        psensor_PushNumeric(ref, 0 /* now */, sample);
    }
    else
    {
        LE_ERROR("Failed to read signal quality (%s).", LE_RESULT_TXT(result));
    }
}

COMPONENT_INIT
{
    // Use the Periodic Sensor component from the Data Hub to implement the sensor interfaces.
    psensor_Create("signalQuality", DHUBIO_DATA_TYPE_NUMERIC, "", SampleSignalQuality, NULL);
}


