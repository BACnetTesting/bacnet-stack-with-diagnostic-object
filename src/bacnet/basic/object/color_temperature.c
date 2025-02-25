/**
 * @file
 * @author Steve Karg
 * @date July 2022
 * @brief Color Temperature object, customize for your use
 *
 * @section DESCRIPTION
 *
 * The Color Temperature object is an object with a present-value that
 * uses an Color Temperature INTEGER type
 *
 * @section LICENSE
 *
 * Copyright (C) 2022 Steve Karg <skarg@users.sourceforge.net>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "bacnet/config.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacerror.h"
#include "bacnet/bacapp.h"
#include "bacnet/bactext.h"
#include "bacnet/cov.h"
#include "bacnet/apdu.h"
#include "bacnet/npdu.h"
#include "bacnet/abort.h"
#include "bacnet/lighting.h"
#include "bacnet/reject.h"
#include "bacnet/rp.h"
#include "bacnet/wp.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/sys/keylist.h"
/* me! */
#include "color_temperature.h"

struct object_data {
    bool Changed : 1;
    bool Write_Enabled : 1;
    uint32_t Present_Value;
    uint32_t Tracking_Value;
    BACNET_COLOR_COMMAND Color_Command;
    BACNET_COLOR_OPERATION_IN_PROGRESS In_Progress;
    uint32_t Default_Color_Temperature;
    uint32_t Default_Fade_Time;
    uint32_t Default_Ramp_Rate;
    uint32_t Default_Step_Increment;
    BACNET_COLOR_TRANSITION Transition;
    uint32_t Present_Value_Minimum;
    uint32_t Present_Value_Maximum;
    const char *Object_Name;
    const char *Description;
};
/* Key List for storing the object data sorted by instance number  */
static OS_Keylist Object_List;
/* callback for present value writes */
static color_temperature_write_present_value_callback
    Color_Temperature_Write_Present_Value_Callback;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Color_Temperature_Properties_Required[] = {
    PROP_OBJECT_IDENTIFIER, PROP_OBJECT_NAME, PROP_OBJECT_TYPE,
    PROP_PRESENT_VALUE, PROP_TRACKING_VALUE, PROP_COLOR_COMMAND,
    PROP_IN_PROGRESS, PROP_DEFAULT_COLOR_TEMPERATURE, PROP_DEFAULT_FADE_TIME,
    PROP_DEFAULT_RAMP_RATE, PROP_DEFAULT_STEP_INCREMENT, -1
};

static const int Color_Temperature_Properties_Optional[] = { PROP_DESCRIPTION,
    PROP_TRANSITION, PROP_MIN_PRES_VALUE, PROP_MAX_PRES_VALUE, -1 };

static const int Color_Temperature_Properties_Proprietary[] = { -1 };

/**
 * Returns the list of required, optional, and proprietary properties.
 * Used by ReadPropertyMultiple service.
 *
 * @param pRequired - pointer to list of int terminated by -1, of
 * BACnet required properties for this object.
 * @param pOptional - pointer to list of int terminated by -1, of
 * BACnet optkional properties for this object.
 * @param pProprietary - pointer to list of int terminated by -1, of
 * BACnet proprietary properties for this object.
 */
void Color_Temperature_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired) {
        *pRequired = Color_Temperature_Properties_Required;
    }
    if (pOptional) {
        *pOptional = Color_Temperature_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Color_Temperature_Properties_Proprietary;
    }

    return;
}

/**
 * Determines if a given Color instance is valid
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  true if the instance is valid, and false if not
 */
bool Color_Temperature_Valid_Instance(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        return true;
    }

    return false;
}

/**
 * Determines the number of Color objects
 *
 * @return  Number of Color objects
 */
unsigned Color_Temperature_Count(void)
{
    return Keylist_Count(Object_List);
}

/**
 * Determines the object instance-number for a given 0..N index
 * of Color objects where N is Color_Temperature_Count().
 *
 * @param  index - 0..N where N is Color_Temperature_Count()
 *
 * @return  object instance-number for the given index
 */
uint32_t Color_Temperature_Index_To_Instance(unsigned index)
{
    return Keylist_Key(Object_List, index);
}

/**
 * For a given object instance-number, determines a 0..N index
 * of Color objects where N is Color_Temperature_Count().
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  index for the given instance-number, or Color_Temperature_Count()
 * if not valid.
 */
unsigned Color_Temperature_Instance_To_Index(uint32_t object_instance)
{
    return Keylist_Index(Object_List, object_instance);
}

/**
 * For a given object instance-number, determines the present-value
 *
 * @param  object_instance - object-instance number of the object
 * @return  present-value of the object
 */
uint32_t Color_Temperature_Present_Value(uint32_t object_instance)
{
    uint32_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Present_Value;
    }

    return value;
}

/**
 * For a given object instance-number, sets the present-value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - floating point Color
 *
 * @return  true if values are within range and present-value is set.
 */
bool Color_Temperature_Present_Value_Set(
    uint32_t object_instance, uint32_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Present_Value = value;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, sets the present-value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - floating point Color
 * @param  priority - priority-array index value 1..16
 * @param  error_class - the BACnet error class
 * @param  error_code - BACnet Error code
 *
 * @return  true if values are within range and present-value is set.
 */
static bool Color_Temperature_Present_Value_Write(uint32_t object_instance,
    uint32_t value,
    uint8_t priority,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    bool status = false;
    struct object_data *pObject;
    uint32_t old_value = 0;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        (void)priority;
        if (pObject->Write_Enabled) {
            old_value = pObject->Present_Value;
            pObject->Present_Value = value;
            if (Color_Temperature_Write_Present_Value_Callback) {
                Color_Temperature_Write_Present_Value_Callback(
                    object_instance, old_value, value);
            }
            status = true;
        } else {
            *error_class = ERROR_CLASS_PROPERTY;
            *error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        }
    } else {
        *error_class = ERROR_CLASS_OBJECT;
        *error_code = ERROR_CODE_UNKNOWN_OBJECT;
    }

    return status;
}

/**
 * For a given object instance-number, determines the present-value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  present-value of the object
 */
uint32_t Color_Temperature_Tracking_Value(uint32_t object_instance)
{
    uint32_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Tracking_Value;
    }

    return value;
}

/**
 * For a given object instance-number, sets the present-value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - floating point Color
 *
 * @return  true if values are within range and present-value is set.
 */
bool Color_Temperature_Tracking_Value_Set(
    uint32_t object_instance, uint32_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Tracking_Value = value;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, determines the present-value
 *
 * @param  object_instance - object-instance number of the object
 * @return  present-value of the object
 */
uint32_t Color_Temperature_Min_Pres_Value(uint32_t object_instance)
{
    uint32_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Present_Value_Minimum;
    }

    return value;
}

/**
 * For a given object instance-number, sets the present-value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - color temperature Kelvin
 *
 * @return  true if values are within range and present-value is set.
 */
bool Color_Temperature_Min_Pres_Value_Set(
    uint32_t object_instance, uint32_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Present_Value_Minimum = value;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, determines the present-value
 *
 * @param  object_instance - object-instance number of the object
 * @return  present-value of the object
 */
uint32_t Color_Temperature_Max_Pres_Value(uint32_t object_instance)
{
    uint32_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Present_Value_Maximum;
    }

    return value;
}

/**
 * For a given object instance-number, sets the present-value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - color temperature Kelvin
 *
 * @return  true if values are within range and present-value is set.
 */
bool Color_Temperature_Max_Pres_Value_Set(
    uint32_t object_instance, uint32_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Present_Value_Maximum = value;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, gets the property value
 *
 * @param object_instance - object-instance number of the object
 * @param value - color command data
 * @return  true if the property value is copied
 */
bool Color_Temperature_Command(
    uint32_t object_instance, BACNET_COLOR_COMMAND *value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && value) {
        color_command_copy(value, &pObject->Color_Command);
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, sets the property value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - color command data
 * @return  true if values are within range and value is set.
 */
bool Color_Temperature_Command_Set(
    uint32_t object_instance, BACNET_COLOR_COMMAND *value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && value) {
        color_command_copy(&pObject->Color_Command, value);
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, gets the property value
 *
 * @param object_instance - object-instance number of the object
 * @return property value
 */
BACNET_COLOR_OPERATION_IN_PROGRESS Color_Temperature_In_Progress(
    uint32_t object_instance)
{
    BACNET_COLOR_OPERATION_IN_PROGRESS value =
        BACNET_COLOR_OPERATION_IN_PROGRESS_MAX;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->In_Progress;
    }

    return value;
}

/**
 * For a given object instance-number, sets the property value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - BACNET_COLOR_OPERATION_IN_PROGRESS
 * @return  true if values are within range and value is set.
 */
bool Color_Temperature_In_Progress_Set(
    uint32_t object_instance, BACNET_COLOR_OPERATION_IN_PROGRESS value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (value < BACNET_COLOR_OPERATION_IN_PROGRESS_MAX) {
            pObject->In_Progress = value;
            status = true;
        }
    }

    return status;
}

/**
 * For a given object instance-number, determines the present-value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  present-value of the object
 */
uint32_t Color_Temperature_Default_Color_Temperature(uint32_t object_instance)
{
    uint32_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Default_Color_Temperature;
    }

    return value;
}

/**
 * For a given object instance-number, sets the present-value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - color temperature Kelvin
 *
 * @return  true if values are within range and present-value is set.
 */
bool Color_Temperature_Default_Color_Temperature_Set(
    uint32_t object_instance, uint32_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Default_Color_Temperature = value;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, gets the property value
 *
 * @param object_instance - object-instance number of the object
 * @return property value
 */
uint32_t Color_Temperature_Default_Fade_Time(uint32_t object_instance)
{
    uint32_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Default_Fade_Time;
    }

    return value;
}

/**
 * For a given object instance-number, sets the property value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - BACNET_COLOR_OPERATION_IN_PROGRESS
 * @return  true if values are within range and value is set.
 */
bool Color_Temperature_Default_Fade_Time_Set(
    uint32_t object_instance, uint32_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if ((value == 0) ||
            ((value >= BACNET_COLOR_FADE_TIME_MIN) &&
                (value <= BACNET_COLOR_FADE_TIME_MAX))) {
            pObject->Default_Fade_Time = value;
        }
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, gets the property value
 *
 * @param object_instance - object-instance number of the object
 * @return property value
 */
uint32_t Color_Temperature_Default_Ramp_Rate(uint32_t object_instance)
{
    uint32_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Default_Ramp_Rate;
    }

    return value;
}

/**
 * For a given object instance-number, sets the property value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - property value
 * @return  true if values are within range and value is set.
 */
bool Color_Temperature_Default_Ramp_Rate_Set(
    uint32_t object_instance, uint32_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Default_Ramp_Rate = value;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, gets the property value
 *
 * @param object_instance - object-instance number of the object
 * @return property value
 */
uint32_t Color_Temperature_Default_Step_Increment(uint32_t object_instance)
{
    uint32_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Default_Step_Increment;
    }

    return value;
}

/**
 * For a given object instance-number, sets the property value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - property value
 * @return  true if values are within range and value is set.
 */
bool Color_Temperature_Default_Step_Increment_Set(
    uint32_t object_instance, uint32_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Default_Step_Increment = value;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, gets the property value
 *
 * @param object_instance - object-instance number of the object
 * @return property value
 */
BACNET_COLOR_TRANSITION Color_Temperature_Transition(uint32_t object_instance)
{
    BACNET_COLOR_TRANSITION value = BACNET_COLOR_TRANSITION_NONE;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Transition;
    }

    return value;
}

/**
 * For a given object instance-number, sets the property value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - BACNET_COLOR_TRANSITION
 * @return  true if values are within range and value is set.
 */
bool Color_Temperature_Transition_Set(
    uint32_t object_instance, BACNET_COLOR_TRANSITION value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (value < BACNET_COLOR_TRANSITION_MAX) {
            pObject->Transition = value;
            status = true;
        }
    }

    return status;
}

/**
 * For a given object instance-number, loads the object-name into
 * a characterstring. Note that the object name must be unique
 * within this device.
 *
 * @param  object_instance - object-instance number of the object
 * @param  object_name - holds the object-name retrieved
 *
 * @return  true if object-name was retrieved
 */
bool Color_Temperature_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    bool status = false;
    struct object_data *pObject;
    char name_text[26] = "COLOR-TEMPERATURE-4194303";

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Object_Name) {
            status =
                characterstring_init_ansi(object_name, pObject->Object_Name);
        } else {
            snprintf(name_text, sizeof(name_text), "COLOR-TEMPERATURE-%u",
                object_instance);
            status = characterstring_init_ansi(object_name, name_text);
        }
    }

    return status;
}

/**
 * For a given object instance-number, sets the object-name
 * Note that the object name must be unique within this device.
 *
 * @param  object_instance - object-instance number of the object
 * @param  new_name - holds the object-name to be set
 *
 * @return  true if object-name was set
 */
bool Color_Temperature_Name_Set(uint32_t object_instance, char *new_name)
{
    bool status = false; /* return value */
    BACNET_CHARACTER_STRING object_name;
    BACNET_OBJECT_TYPE found_type = 0;
    uint32_t found_instance = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && new_name) {
        /* All the object names in a device must be unique */
        characterstring_init_ansi(&object_name, new_name);
        if (Device_Valid_Object_Name(
                &object_name, &found_type, &found_instance)) {
            if ((found_type == OBJECT_COLOR) &&
                (found_instance == object_instance)) {
                /* writing same name to same object */
                status = true;
            } else {
                /* duplicate name! */
                status = false;
            }
        } else {
            status = true;
            pObject->Object_Name = new_name;
            Device_Inc_Database_Revision();
        }
    }

    return status;
}

/**
 * For a given object instance-number, returns the description
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return description text or NULL if not found
 */
char *Color_Temperature_Description(uint32_t object_instance)
{
    char *name = NULL;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Description) {
            name = (char *)pObject->Description;
        } else {
            name = "";
        }
    }

    return name;
}

/**
 * For a given object instance-number, sets the description
 *
 * @param  object_instance - object-instance number of the object
 * @param  new_name - holds the description to be set
 *
 * @return  true if object-name was set
 */
bool Color_Temperature_Description_Set(uint32_t object_instance, char *new_name)
{
    bool status = false; /* return value */
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && new_name) {
        status = true;
        pObject->Description = new_name;
    }

    return status;
}

/**
 * ReadProperty handler for this object.  For the given ReadProperty
 * data, the application_data is loaded or the error flags are set.
 *
 * @param  rpdata - BACNET_READ_PROPERTY_DATA data, including
 * requested data and space for the reply, or error response.
 *
 * @return number of APDU bytes in the response, or
 * BACNET_STATUS_ERROR on error.
 */
int Color_Temperature_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int apdu_len = 0; /* return value */
    BACNET_CHARACTER_STRING char_string;
    uint8_t *apdu = NULL;
    BACNET_COLOR_COMMAND color_command = { 0 };

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }

    apdu = rpdata->application_data;
    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], rpdata->object_type, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
            Color_Temperature_Object_Name(
                rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0], rpdata->object_type);
            break;
        case PROP_PRESENT_VALUE:
            apdu_len = encode_application_unsigned(
                apdu, Color_Temperature_Present_Value(rpdata->object_instance));
            break;
        case PROP_MIN_PRES_VALUE:
            apdu_len = encode_application_unsigned(apdu,
                Color_Temperature_Min_Pres_Value(rpdata->object_instance));
            break;
        case PROP_MAX_PRES_VALUE:
            apdu_len = encode_application_unsigned(apdu,
                Color_Temperature_Max_Pres_Value(rpdata->object_instance));
            break;
        case PROP_TRACKING_VALUE:
            apdu_len = encode_application_unsigned(apdu,
                Color_Temperature_Tracking_Value(rpdata->object_instance));
            break;
        case PROP_COLOR_COMMAND:
            if (Color_Temperature_Command(
                    rpdata->object_instance, &color_command)) {
                apdu_len = color_command_encode(apdu, &color_command);
            }
            break;
        case PROP_IN_PROGRESS:
            apdu_len = encode_application_enumerated(
                apdu, Color_Temperature_In_Progress(rpdata->object_instance));
            break;
        case PROP_DEFAULT_COLOR_TEMPERATURE:
            apdu_len = encode_application_unsigned(apdu,
                Color_Temperature_Default_Color_Temperature(
                    rpdata->object_instance));
            break;
        case PROP_DEFAULT_FADE_TIME:
            apdu_len = encode_application_unsigned(apdu,
                Color_Temperature_Default_Fade_Time(rpdata->object_instance));
            break;
        case PROP_DEFAULT_RAMP_RATE:
            apdu_len = encode_application_unsigned(apdu,
                Color_Temperature_Default_Ramp_Rate(rpdata->object_instance));
            break;
        case PROP_DEFAULT_STEP_INCREMENT:
            apdu_len = encode_application_unsigned(apdu,
                Color_Temperature_Default_Step_Increment(
                    rpdata->object_instance));
            break;
        case PROP_TRANSITION:
            apdu_len = encode_application_enumerated(
                apdu, Color_Temperature_Transition(rpdata->object_instance));
            break;
        case PROP_DESCRIPTION:
            characterstring_init_ansi(&char_string,
                Color_Temperature_Description(rpdata->object_instance));
            apdu_len = encode_application_character_string(apdu, &char_string);
            break;
        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }
    /*  only array properties can have array options */
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_PRIORITY_ARRAY) &&
        (rpdata->object_property != PROP_EVENT_TIME_STAMPS) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/**
 * WriteProperty handler for this object.  For the given WriteProperty
 * data, the application_data is loaded or the error flags are set.
 *
 * @param  wp_data - BACNET_WRITE_PROPERTY_DATA data, including
 * requested data and space for the reply, or error response.
 *
 * @return false if an error is loaded, true if no errors
 */
bool Color_Temperature_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;

    /* decode the some of the request */
    len = bacapp_decode_application_data(
        wp_data->application_data, wp_data->application_data_len, &value);
    /* FIXME: len < application_data_len: more data? */
    if (len < 0) {
        /* error while decoding - a value larger than we can handle */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }
    if ((wp_data->object_property != PROP_PRIORITY_ARRAY) &&
        (wp_data->object_property != PROP_EVENT_TIME_STAMPS) &&
        (wp_data->array_index != BACNET_ARRAY_ALL)) {
        /*  only array properties can have array options */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }
    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                status = Color_Temperature_Present_Value_Write(
                    wp_data->object_instance, value.type.Unsigned_Int,
                    wp_data->priority, &wp_data->error_class,
                    &wp_data->error_code);
            }
            break;
        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_TYPE:
        case PROP_OBJECT_NAME:
        case PROP_DESCRIPTION:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        default:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            break;
    }

    return status;
}

/**
 * @brief Sets a callback used when present-value is written from BACnet
 * @param cb - callback used to provide indications
 */
void Color_Temperature_Write_Present_Value_Callback_Set(
    color_temperature_write_present_value_callback cb)
{
    Color_Temperature_Write_Present_Value_Callback = cb;
}

/**
 * @brief Determines a object write-enabled flag state
 * @param object_instance - object-instance number of the object
 * @return  write-enabled status flag
 */
bool Color_Temperature_Write_Enabled(uint32_t object_instance)
{
    bool value = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Write_Enabled;
    }

    return value;
}

/**
 * @brief For a given object instance-number, sets the write-enabled flag
 * @param object_instance - object-instance number of the object
 */
void Color_Temperature_Write_Enable(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Write_Enabled = true;
    }
}

/**
 * @brief For a given object instance-number, clears the write-enabled flag
 * @param object_instance - object-instance number of the object
 */
void Color_Temperature_Write_Disable(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Write_Enabled = false;
    }
}

/**
 * @brief Creates a Color Temperature object
 * @param object_instance - object-instance number of the object
 * @return the object-instance that was created, or BACNET_MAX_INSTANCE
 */
uint32_t Color_Temperature_Create(uint32_t object_instance)
{
    struct object_data *pObject = NULL;
    int index = 0;

    if (object_instance > BACNET_MAX_INSTANCE) {
        return BACNET_MAX_INSTANCE;
    } else if (object_instance == BACNET_MAX_INSTANCE) {
        /* wildcard instance */
        /* the Object_Identifier property of the newly created object 
            shall be initialized to a value that is unique within the 
            responding BACnet-user device. The method used to generate 
            the object identifier is a local matter.*/
        object_instance = Keylist_Next_Empty_Key(Object_List, 1);
    }
    pObject = Keylist_Data(Object_List, object_instance);
    if (!pObject) {
        pObject = calloc(1, sizeof(struct object_data));
        if (pObject) {
            pObject->Object_Name = NULL;
            pObject->Present_Value = 0;
            pObject->Tracking_Value = 0;
            pObject->Color_Command.operation = BACNET_COLOR_OPERATION_NONE;
            pObject->In_Progress = BACNET_COLOR_OPERATION_IN_PROGRESS_IDLE;
            pObject->Default_Color_Temperature = 5000;
            pObject->Default_Fade_Time = 0;
            pObject->Default_Ramp_Rate = 0;
            pObject->Default_Step_Increment = 0;
            pObject->Transition = BACNET_COLOR_TRANSITION_NONE;
            pObject->Present_Value_Minimum = 0;
            pObject->Present_Value_Maximum = 0;
            pObject->Changed = false;
            pObject->Write_Enabled = false;
            /* add to list */
            index = Keylist_Data_Add(Object_List, object_instance, pObject);
            if (index >= 0) {
                Device_Inc_Database_Revision();
            } else {
                free(pObject);
                return BACNET_MAX_INSTANCE;
            }
        } else {
            return BACNET_MAX_INSTANCE;
        }
    }

    return object_instance;
}

/**
 * Deletes an Color object
 * @param object_instance - object-instance number of the object
 * @return true if the object is deleted
 */
bool Color_Temperature_Delete(uint32_t object_instance)
{
    bool status = false;
    struct object_data *pObject = NULL;

    pObject = Keylist_Data_Delete(Object_List, object_instance);
    if (pObject) {
        free(pObject);
        status = true;
        Device_Inc_Database_Revision();
    }

    return status;
}

/**
 * Deletes all the Colors and their data
 */
void Color_Temperature_Cleanup(void)
{
    struct object_data *pObject;

    if (Object_List) {
        do {
            pObject = Keylist_Data_Pop(Object_List);
            if (pObject) {
                free(pObject);
                Device_Inc_Database_Revision();
            }
        } while (pObject);
        Keylist_Delete(Object_List);
        Object_List = NULL;
    }
}

/**
 * Initializes the Color object data
 */
void Color_Temperature_Init(void)
{
    Object_List = Keylist_Create();
    if (Object_List) {
        atexit(Color_Temperature_Cleanup);
    }
}
