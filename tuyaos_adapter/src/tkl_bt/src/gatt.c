#include "tkl_bluetooth.h"

#include "tkl_mutex.h"
#include "tuya_slist.h"

#include "bluetooth_api.h"
#include <systemd/sd-bus.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAN_PATH "/"

#define BUS_NAME             "org.bluez"
#define SERVICE_INTERFACE    "org.bluez.GattService1"
#define CHRC_INTERFACE       "org.bluez.GattCharacteristic1"
#define GATT_MANAGER_IFACE   "org.bluez.GattManager1"
#define OBJECT_MANAGER_IFACE "org.freedesktop.DBus.ObjectManager"
#define PROPERTIES_IFACE     "org.freedesktop.DBus.Properties"

#define CHRC_FLAGS_BROADCAST                   "broadcast"
#define CHRC_FLAGS_READ                        "read"
#define CHRC_FLAGS_WRITE_WITHOUT_RESPONSE      "write-without-response"
#define CHRC_FLAGS_WRITE                       "write"
#define CHRC_FLAGS_NOTIFY                      "notify"
#define CHRC_FLAGS_INDICATE                    "indicate"
#define CHRC_FLAGS_AUTHENTICATED_SIGNED_WRITES "authenticated-signed-writes"
#define CHRC_FLAGS_EXTENDED_PROPERTIES         "extended-properties"

#define CHRC_FLAGS_RELIABLE_WRITE                 "reliable-write"
#define CHRC_FLAGS_WRITABLE_AUXILIARIES           "writable-auxiliaries"
#define CHRC_FLAGS_ENCRYPT_READ                   "encrypt-read"
#define CHRC_FLAGS_ENCRYPT_WRITE                  "encrypt-write"
#define CHRC_FLAGS_ENCRYPT_NOTIFY                 "encrypt-notify"   // (Server only)
#define CHRC_FLAGS_ENCRYPT_INDICATE               "encrypt-indicate" // (Server only)
#define CHRC_FLAGS_ENCRYPT_AUTHENTICATED_READ     "encrypt-authenticated-read"
#define CHRC_FLAGS_ENCRYPT_AUTHENTICATED_WRITE    "encrypt-authenticated-write"
#define CHRC_FLAGS_ENCRYPT_AUTHENTICATED_NOTIFY   "encrypt-authenticated-notify"   // (Server only)
#define CHRC_FLAGS_ENCRYPT_AUTHENTICATED_INDICATE "encrypt-authenticated-indicate" // (Server only)
#define CHRC_FLAGS_SECURE_READ                    "secure-read"                    // (Server only)
#define CHRC_FLAGS_SECURE_WRITE                   "secure-write"                   // (Server only)
#define CHRC_FLAGS_SECURE_NOTIFY                  "secure-notify"                  // (Server only)
#define CHRC_FLAGS_SECURE_INDICATE                "secure-indicate"                // (Server only)
#define CHRC_FLAGS_AUTHORIZE                      "authorize"

#define SERVICE_CHRC_PATH_MAX_LENGTH 35
#define LOCAL_SERVICES_NUM           3
#define LOCAL_CHRCS_NUM              6
typedef struct {
    CHAR_T *path;
    CHAR_T *uuid;
    BYTE_T *data;
    SIZE_T data_len;
    CHAR_T **flags;
    SLIST_HEAD node;
} chrc_t;
typedef struct {
    CHAR_T *path;
    CHAR_T *uuid;
    INT_T primary;
    SLIST_HEAD chrcs;
    SLIST_HEAD node;
} service_t;
typedef struct {
    CHAR_T *path;
    CHAR_T *address;
    CHAR_T *name;
    INT16_T rssi;
    INT_T connected;
    SLIST_HEAD services;
    SLIST_HEAD node;
} device_t;

typedef struct {
    CHAR_T svc_path[SERVICE_CHRC_PATH_MAX_LENGTH];
    CHAR_T path[SERVICE_CHRC_PATH_MAX_LENGTH];
    TKL_BLE_CHAR_PARAMS_T *para;
    BOOL_T notifying;
    BYTE_T data[512];
    SIZE_T data_len;
} local_chrc_t;

typedef struct local_service_t {
    CHAR_T path[SERVICE_CHRC_PATH_MAX_LENGTH];
    USHORT_T handle;
    TKL_BLE_UUID_T svc_uuid;
    TKL_BLE_SERVICE_TYPE_E type;
    local_chrc_t chrcs[LOCAL_CHRCS_NUM];
} local_service_t;

CHAR_T *BLUEZ_DEVICE_PATH = "/org/bluez/hci0";

STATIC sd_bus *bus = NULL;
STATIC pthread_t bus_task_thId;

STATIC pthread_t gatt_evt_task_thId;
STATIC TKL_BLE_GATT_EVT_FUNC_CB g_gatt_evt_cb = NULL;

STATIC TKL_MUTEX_HANDLE ble_devices_mutex;
STATIC BOOL_T is_service_add                              = FALSE;
STATIC local_service_t local_services[LOCAL_SERVICES_NUM] = { 0 };
STATIC SLIST_HEAD(ble_devices);
STATIC UINT_T ble_connected_device_num = 0;

STATIC CHAR_T *write_chrc_flag[]  = { CHRC_FLAGS_WRITE, CHRC_FLAGS_WRITE_WITHOUT_RESPONSE, NULL };
STATIC CHAR_T *notify_chrc_flag[] = { CHRC_FLAGS_NOTIFY, NULL };
STATIC CHAR_T *read_chrc_flag[]   = { CHRC_FLAGS_READ, NULL };

VOID_T uuid_to_string(TKL_BLE_UUID_T *uuid, CHAR_T *str, size_t n)
{
    UCHAR_T *uuid128;

    switch (uuid->uuid_type) {
    case TKL_BLE_UUID_TYPE_16:
        snprintf(str, n, "0x%.4x", uuid->uuid.uuid16);
        break;
    case TKL_BLE_UUID_TYPE_32:
        snprintf(str, n, "0x%.8x", uuid->uuid.uuid32);
        break;
    case TKL_BLE_UUID_TYPE_128:
        uuid128 = uuid->uuid.uuid128;
        snprintf(str, n, "%.2x%.2x%.2x%.2x-%.2x%.2x-%.2x%.2x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x",
                 uuid128[15], uuid128[14], uuid128[13], uuid128[12],
                 uuid128[11], uuid128[10],
                 uuid128[9], uuid128[8],
                 uuid128[7], uuid128[6],
                 uuid128[5], uuid128[4], uuid128[3], uuid128[2], uuid128[1], uuid128[0]);
        break;
    default:
        break;
    }
}

STATIC INT_T chrc_props(sd_bus *bus, CONST CHAR_T *path, CONST CHAR_T *interface,
                        CONST CHAR_T *property, sd_bus_message *reply, VOID_T *userdata,
                        sd_bus_error *ret_error)
{
    local_chrc_t *chrc = (local_chrc_t *)userdata;

    if (strcmp(property, "UUID") == 0) {
        UCHAR_T uuid[37] = { 0 };
        uuid_to_string(&(chrc->para->char_uuid), uuid, 37);
        return sd_bus_message_append(reply, "s", uuid);
    }

    if (strcmp(property, "Service") == 0) {
        return sd_bus_message_append(reply, "o", chrc->svc_path);
    }

    if (strcmp(property, "Value") == 0) {
        return sd_bus_message_append_array(reply, 'y', chrc->data, chrc->data_len);
    }

    if (strcmp(property, "Flags") == 0) {
        if (chrc->para->property & TKL_BLE_GATT_CHAR_PROP_WRITE) {
            return sd_bus_message_append_strv(reply, write_chrc_flag);
        } else if (chrc->para->property & TKL_BLE_GATT_CHAR_PROP_NOTIFY) {
            return sd_bus_message_append_strv(reply, notify_chrc_flag);
        } else if (chrc->para->property & TKL_BLE_GATT_CHAR_PROP_READ) {
            return sd_bus_message_append_strv(reply, read_chrc_flag);
        }
    }

    if (strcmp(property, "Handle") == 0) {
        return sd_bus_message_append(reply, "q", chrc->para->handle);
    }

    if (strcmp(property, "Notifying") == 0) {
        return sd_bus_message_append(reply, "b", chrc->notifying);
    }

    return -2;
}

STATIC INT_T chrc_set_handle(sd_bus *bus, CONST CHAR_T *path, CONST CHAR_T *interface,
                             CONST CHAR_T *property, sd_bus_message *reply, VOID_T *userdata,
                             sd_bus_error *ret_error)
{
    local_chrc_t *chrc = (local_chrc_t *)userdata;

    if (strcmp(property, "Handle") == 0) {
        return sd_bus_message_read(reply, "q", &(chrc->para->handle));
    }

    return -2;
}

STATIC INT_T chrc_readvalue(sd_bus_message *m, VOID_T *userdata, sd_bus_error *ret_error)
{
    local_chrc_t *chrc = (local_chrc_t *)userdata;
    sd_bus_message *reply;
    INT_T op_ret;

    IF_FAIL_RETURN(sd_bus_message_new_method_return(m, &reply));
    IF_FAIL_RETURN(sd_bus_message_append_array(reply, 'y', chrc->data, chrc->data_len));

    return sd_bus_send(sd_bus_message_get_bus(m), reply, NULL);
}

STATIC INT_T chrc_writevalue(sd_bus_message *m, VOID_T *userdata, sd_bus_error *ret_error)
{
    BYTE_T *tmp_data   = NULL;
    local_chrc_t *chrc = (local_chrc_t *)userdata;
    INT_T op_ret;

    IF_FAIL_RETURN(sd_bus_message_has_signature(m, "aya{sv}"));
    IF_FAIL_RETURN(sd_bus_message_read_array(m, 'y', (CONST VOID_T **)&tmp_data, &chrc->data_len));

    if (chrc->data_len > 512) {
        chrc->data_len = 512;
    }
    memcpy(chrc->data, tmp_data, chrc->data_len);
    // free(tmp_data);

    TKL_BLE_GATT_PARAMS_EVT_T *evt = (TKL_BLE_GATT_PARAMS_EVT_T *)malloc(sizeof(TKL_BLE_GATT_PARAMS_EVT_T));
    if (evt == NULL) {
        return -1;
    }
    memset(evt, 0x00, sizeof(TKL_BLE_GATT_PARAMS_EVT_T));

    evt->type = TKL_BLE_GATT_EVT_WRITE_REQ;
    // evt->conn_handle = ;
    evt->gatt_event.write_report.char_handle   = chrc->para->handle;
    evt->gatt_event.write_report.report.length = chrc->data_len;
    evt->gatt_event.write_report.report.p_data = chrc->data;
    if (g_gatt_evt_cb) {
        g_gatt_evt_cb(evt);
    }

    return sd_bus_reply_method_return(m, "");
}

STATIC INT_T chrc_startnotify(sd_bus_message *m, VOID_T *userdata, sd_bus_error *ret_error)
{
    local_chrc_t *chrc = (local_chrc_t *)userdata;
    chrc->notifying    = TRUE;

    return sd_bus_reply_method_return(m, "");
}

STATIC INT_T chrc_stopnotify(sd_bus_message *m, VOID_T *userdata, sd_bus_error *ret_error)
{
    local_chrc_t *chrc = (local_chrc_t *)userdata;
    chrc->notifying    = FALSE;

    return sd_bus_reply_method_return(m, "");
}

STATIC INT_T svc_props(sd_bus *bus, CONST CHAR_T *path, CONST CHAR_T *interface,
                       CONST CHAR_T *property, sd_bus_message *reply, VOID_T *userdata,
                       sd_bus_error *ret_error)
{
    local_service_t *service = (local_service_t *)userdata;

    if (strcmp(property, "UUID") == 0) {
        UCHAR_T uuid[37] = { 0 };
        uuid_to_string(&(service->svc_uuid), uuid, 37);
        return sd_bus_message_append(reply, "s", uuid);
    }

    if (strcmp(property, "Primary") == 0) {
        if (service->type == TKL_BLE_UUID_SERVICE_PRIMARY) {
            return sd_bus_message_append(reply, "b", TRUE);
        } else {
            return sd_bus_message_append(reply, "b", FALSE);
        }
    }

    if (strcmp(property, "Handle") == 0) {
        return sd_bus_message_append(reply, "q", service->handle);
    }

    return -2;
}

STATIC INT_T svc_set_handle(sd_bus *bus, CONST CHAR_T *path, CONST CHAR_T *interface,
                            CONST CHAR_T *property, sd_bus_message *reply, VOID_T *userdata,
                            sd_bus_error *ret_error)
{
    local_service_t *service = (local_service_t *)userdata;

    if (strcmp(property, "Handle") == 0) {
        return sd_bus_message_read(reply, "q", &(service->handle));
    }

    return -2;
}

STATIC CONST sd_bus_vtable svc_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("UUID", "s", svc_props, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Primary", "b", svc_props, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    // SD_BUS_PROPERTY("Device",   "o",  svc_props, 0, SD_BUS_VTABLE_PROPERTY_CONST), // optional
    // SD_BUS_PROPERTY("Includes", "ao", svc_props, 0, SD_BUS_VTABLE_PROPERTY_CONST), // optional
    SD_BUS_WRITABLE_PROPERTY("Handle", "q", svc_props, svc_set_handle, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE), // optional
    SD_BUS_VTABLE_END
};

STATIC CONST sd_bus_vtable chrc_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("UUID", "s", chrc_props, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Service", "o", chrc_props, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Value", "ay", chrc_props, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    // SD_BUS_PROPERTY("WriteAcquired",  "b",  chrc_props, 0, SD_BUS_VTABLE_PROPERTY_CONST), // optional
    // SD_BUS_PROPERTY("NotifyAcquired", "b",  chrc_props, 0, SD_BUS_VTABLE_PROPERTY_CONST), // optional
    SD_BUS_PROPERTY("Notifying", "b", chrc_props, 0, SD_BUS_VTABLE_PROPERTY_CONST), // optional
    SD_BUS_PROPERTY("Flags", "as", chrc_props, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_WRITABLE_PROPERTY("Handle", "q", chrc_props, chrc_set_handle, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE), // optional

    SD_BUS_METHOD("ReadValue", "a{sv}", "ay", chrc_readvalue, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("WriteValue", "aya{sv}", "", chrc_writevalue, SD_BUS_VTABLE_UNPRIVILEGED),
    // SD_BUS_METHOD("AcquireWrite",  "a{sv}",   "",   chrc_notsupport,  SD_BUS_VTABLE_UNPRIVILEGED), // optional
    // SD_BUS_METHOD("AcquireNotify", "a{sv}",   "",   chrc_notsupport,  SD_BUS_VTABLE_UNPRIVILEGED), // optional
    SD_BUS_METHOD("StartNotify", "", "", chrc_startnotify, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("StopNotify", "", "", chrc_stopnotify, SD_BUS_VTABLE_UNPRIVILEGED),
    // SD_BUS_METHOD("Confirm",       "",        "",   chrc_notsupport,  SD_BUS_VTABLE_UNPRIVILEGED), // optional
    SD_BUS_VTABLE_END
};

STATIC INT_T copy_str(VOID_T **dst, CONST VOID_T *src)
{
    INT_T len;

    if (!src || *dst) {
        return -1;
    }

    len  = strlen(src);
    *dst = (VOID_T *)malloc(len + 1);
    if (!dst) {
        return -1;
    }
    memset(*dst, 0x00, len + 1);
    memcpy(*dst, src, len);

    return len;
}

STATIC INT_T properties_changed(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    INT_T op_ret;
    CHAR_T *iface;
    CHAR_T *key;
    device_t *device = (device_t *)userdata;

    IF_FAIL_RETURN(sd_bus_message_has_signature(m, "sa{sv}as"));
    IF_FAIL_RETURN(sd_bus_message_read(m, "s", &iface));
    if (strcmp(iface, "org.bluez.Device1") != 0) {
        return 0;
    }

    IF_FAIL_RETURN(sd_bus_message_enter_container(m, 'a', "{sv}"));

    while ((op_ret = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
        IF_FAIL_RETURN(sd_bus_message_read(m, "s", &key));

        if (strcmp(key, "RSSI") == 0) {
            IF_FAIL_RETURN(sd_bus_message_read(m, "v", "n", &device->rssi));
        } else if (strcmp(key, "Name") == 0) {
            IF_FAIL_RETURN(sd_bus_message_read(m, "v", "s", &device->name));
        } else if (strcmp(key, "Connected") == 0) {
            INT_T temp = 0;
            IF_FAIL_RETURN(sd_bus_message_read(m, "v", "b", &temp));
            if (temp) {
                device->connected = temp;
                ble_connected_device_num++;
            } else {
                if (device->connected) {
                    device->connected = temp;
                    ble_connected_device_num--;
                }
            }
        } else {
            IF_FAIL_RETURN(sd_bus_message_skip(m, "v"));
        }
        IF_FAIL_RETURN(sd_bus_message_exit_container(m));
    }
    if (op_ret < 0) {
        return op_ret;
    }
    IF_FAIL_RETURN(sd_bus_message_skip(m, "as"));
    IF_FAIL_RETURN(sd_bus_message_exit_container(m));

    return op_ret;
}

STATIC INT_T add_device(sd_bus_message *m, CONST CHAR_T *obj)
{
    INT_T op_ret;
    CONST CHAR_T *key;
    device_t *device = NULL;

    device = (device_t *)malloc(sizeof(device_t));
    if (!device) {
        return -1;
    }
    memset(device, 0x00, sizeof(device_t));

    if (copy_str((VOID_T **)&device->path, obj) < 0) {
        free(device);
        device = NULL;

        return -1;
    }

    IF_FAIL_RETURN_FREE(sd_bus_message_enter_container(m, 'a', "{sv}"), device);

    while ((op_ret = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
        IF_FAIL_RETURN_FREE(sd_bus_message_read(m, "s", &key), device);

        if (strcmp(key, "Address") == 0) {
            IF_FAIL_RETURN_FREE(sd_bus_message_read(m, "v", "s", &device->address), device);
        } else if (strcmp(key, "Name") == 0) {
            IF_FAIL_RETURN_FREE(sd_bus_message_read(m, "v", "s", &device->name), device);
        } else if (strcmp(key, "Connected") == 0) {
            IF_FAIL_RETURN_FREE(sd_bus_message_read(m, "v", "b", &device->connected), device);

            if (device->connected) {
                ble_connected_device_num++;
            }
        } else {
            IF_FAIL_RETURN_FREE(sd_bus_message_skip(m, "v"), device);
        }
        IF_FAIL_RETURN_FREE(sd_bus_message_exit_container(m), device);
    }
    if (op_ret < 0) {
        free(device);
        device = NULL;
        return op_ret;
    }

    IF_FAIL_RETURN_FREE(sd_bus_message_exit_container(m), device);
    IF_FAIL_RETURN_FREE(sd_bus_match_signal(sd_bus_message_get_bus(m), NULL, BUS_NAME, device->path,
                                            PROPERTIES_IFACE, "PropertiesChanged", properties_changed, device),
                        device);

    // tuya_slist_add_head(&ble_device, &device->node);
    tuya_slist_add_tail(&ble_devices, &device->node);

    return op_ret;
}

STATIC INT_T add_service(sd_bus_message *m, CONST CHAR_T *obj)
{
    device_t *device   = NULL;
    service_t *service = NULL;
    SLIST_HEAD *pos    = NULL;
    BOOL_T is_found    = FALSE;
    INT_T op_ret;
    CONST CHAR_T *key;

    // 寻找设备挂载点
    SLIST_FOR_EACH_ENTRY(device, device_t, pos, &ble_devices, node)
    {
        if (strstr(obj, device->path)) {
            is_found = TRUE;
            break;
        }
    }

    if (FALSE == is_found) {
        return -1;
    }

    service = (service_t *)malloc(sizeof(service_t));
    if (!service) {
        return -1;
    }
    memset(service, 0x00, sizeof(service_t));
    if (copy_str((VOID_T **)&service->path, obj) < 0) {
        free(service);
        service = NULL;

        return -1;
    }

    IF_FAIL_RETURN_FREE(sd_bus_message_enter_container(m, 'a', "{sv}"), service);

    while ((op_ret = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
        IF_FAIL_RETURN_FREE(sd_bus_message_read(m, "s", &key), service);

        if (strcmp(key, "UUID") == 0) {
            IF_FAIL_RETURN_FREE(sd_bus_message_read(m, "v", "s", &service->uuid), service);
        } else if (strcmp(key, "Primary") == 0) {
            IF_FAIL_RETURN_FREE(sd_bus_message_read(m, "v", "b", &service->primary), service);
        } else {
            IF_FAIL_RETURN_FREE(sd_bus_message_skip(m, "v"), service);
        }
        IF_FAIL_RETURN_FREE(sd_bus_message_exit_container(m), service);
    }
    if (op_ret < 0) {
        free(service);
        service = NULL;
        return op_ret;
    }

    IF_FAIL_RETURN_FREE(sd_bus_message_exit_container(m), service);

    // tuya_slist_add_head(&device->services, &service->node);
    tuya_slist_add_tail(&device->services, &service->node);

    return op_ret;
}

STATIC INT_T add_chrc(sd_bus_message *m, CONST CHAR_T *obj)
{
    device_t *device   = NULL;
    chrc_t *chrc       = NULL;
    service_t *service = NULL;
    SLIST_HEAD *pos    = NULL;
    BOOL_T is_found    = FALSE;
    INT_T op_ret;
    CONST CHAR_T *key;

    // 寻找设备挂载点
    SLIST_FOR_EACH_ENTRY(device, device_t, pos, &ble_devices, node)
    {
        if (strstr(obj, device->path)) {
            is_found = TRUE;
            break;
        }
    }
    if (FALSE == is_found) {
        return -1;
    }
    is_found = FALSE;
    // 寻找服务挂载点
    SLIST_FOR_EACH_ENTRY(service, service_t, pos, &device->services, node)
    {
        if (strstr(obj, service->path)) {
            is_found = TRUE;
            break;
        }
    }
    if (FALSE == is_found) {
        return -1;
    }

    chrc = (chrc_t *)malloc(sizeof(chrc_t));
    if (!chrc) {
        return -1;
    }
    memset(chrc, 0x00, sizeof(chrc_t));
    if (copy_str((VOID_T **)&chrc->path, obj) < 0) {
        free(chrc);
        chrc = NULL;

        return -1;
    }

    IF_FAIL_RETURN_FREE(sd_bus_message_enter_container(m, 'a', "{sv}"), chrc);

    while ((op_ret = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
        IF_FAIL_RETURN_FREE(sd_bus_message_read(m, "s", &key), chrc);

        if (strcmp(key, "UUID") == 0) {
            IF_FAIL_RETURN_FREE(sd_bus_message_read(m, "v", "s", &chrc->uuid), chrc);
        } else if (strcmp(key, "Value") == 0) {
            IF_FAIL_RETURN_FREE(sd_bus_message_enter_container(m, 'v', "ay"), chrc);
            IF_FAIL_RETURN_FREE(sd_bus_message_read_array(m, 'y', (CONST VOID_T **)&chrc->data, &chrc->data_len), chrc);
            IF_FAIL_RETURN_FREE(sd_bus_message_exit_container(m), chrc);
        } else if (strcmp(key, "Flags") == 0) {
            IF_FAIL_RETURN_FREE(sd_bus_message_enter_container(m, 'v', "as"), chrc);
            IF_FAIL_RETURN_FREE(sd_bus_message_read_strv(m, &chrc->flags), chrc);
            IF_FAIL_RETURN_FREE(sd_bus_message_exit_container(m), chrc);
        } else {
            IF_FAIL_RETURN_FREE(sd_bus_message_skip(m, "v"), chrc);
        }
        IF_FAIL_RETURN_FREE(sd_bus_message_exit_container(m), chrc);
    }
    if (op_ret < 0) {
        free(chrc);
        chrc = NULL;
        return op_ret;
    }

    IF_FAIL_RETURN_FREE(sd_bus_message_exit_container(m), chrc);

    // tuya_slist_add_head(&service->chrcs, &chrc->node);
    tuya_slist_add_tail(&service->chrcs, &chrc->node);

    return op_ret;
}

STATIC INT_T on_bt_iface_add(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    CONST CHAR_T *obj = NULL;
    INT_T op_ret;

    IF_FAIL_RETURN(sd_bus_message_has_signature(m, "oa{sa{sv}}"));
    IF_FAIL_RETURN(sd_bus_message_read(m, "o", &obj));
    /* check if it is below our own device path */
    if ((strlen(obj) <= strlen(BLUEZ_DEVICE_PATH)) || strncmp(obj, BLUEZ_DEVICE_PATH, strlen(BLUEZ_DEVICE_PATH))) {
        IF_FAIL_RETURN(sd_bus_message_skip(m, "a{sa{sv}}"));
        return 0;
    }

    IF_FAIL_RETURN(sd_bus_message_enter_container(m, 'a', "{sa{sv}}"));

    while ((op_ret = sd_bus_message_enter_container(m, 'e', "sa{sv}")) > 0) {
        CONST CHAR_T *iface = NULL;
        bool skip           = true;

        IF_FAIL_RETURN(sd_bus_message_read(m, "s", &iface));

        if (strcmp(iface, "org.bluez.Device1") == 0) {
            tkl_mutex_lock(ble_devices_mutex);
            add_device(m, obj);
            tkl_mutex_unlock(ble_devices_mutex);
            skip = false;
        } else if (strcmp(iface, "org.bluez.GattService1") == 0) {
            tkl_mutex_lock(ble_devices_mutex);
            add_service(m, obj);
            tkl_mutex_unlock(ble_devices_mutex);
            skip = false;
        } else if (strcmp(iface, "org.bluez.GattCharacteristic1") == 0) {
            tkl_mutex_lock(ble_devices_mutex);
            add_chrc(m, obj);
            tkl_mutex_unlock(ble_devices_mutex);
            skip = false;
        }

        if (skip) {
            IF_FAIL_RETURN(sd_bus_message_skip(m, "a{sv}"));
        }
        IF_FAIL_RETURN(sd_bus_message_exit_container(m));
    }
    if (op_ret < 0) {
        return op_ret;
    }

    IF_FAIL_RETURN(sd_bus_message_exit_container(m));

    return 0;
}

STATIC INT_T add_already_known_device(sd_bus *bus)
{
    INT_T op_ret;
    sd_bus_message *m = NULL;

    IF_FAIL_RETURN(sd_bus_call_method(bus, BUS_NAME, MAN_PATH, OBJECT_MANAGER_IFACE,
                                      "GetManagedObjects", NULL, &m, ""));
    IF_FAIL_RETURN(sd_bus_message_enter_container(m, 'a', "{oa{sa{sv}}}"));

    while ((op_ret = sd_bus_message_enter_container(m, 'e', "oa{sa{sv}}")) > 0) {
        IF_FAIL_RETURN(on_bt_iface_add(m, bus, NULL));
        IF_FAIL_RETURN(sd_bus_message_exit_container(m));
    }
    if (op_ret < 0) {
        return op_ret;
    }

    IF_FAIL_RETURN(sd_bus_message_exit_container(m));

    return 0;
}

STATIC VOID_T remove_chrc(chrc_t *chrc)
{
    free(chrc->path);
    chrc->path = NULL;

    // free(chrc->uuid);
    // chrc->uuid = NULL;

    // free(chrc->data);
    // chrc->data = NULL;

    free(chrc->flags);
    chrc->flags = NULL;

    free(chrc);
    chrc = NULL;
}

STATIC VOID_T remove_service(service_t *service)
{
    chrc_t *chrc    = NULL;
    SLIST_HEAD *pos = NULL;

    free(service->path);
    service->path = NULL;

    // free(service->uuid);
    // service->uuid = NULL;

    SLIST_FOR_EACH_ENTRY(chrc, chrc_t, pos, &service->chrcs, node)
    {
        remove_chrc(chrc);
    }

    free(service);
    service = NULL;
}

STATIC VOID_T remove_device(device_t *device)
{
    service_t *service = NULL;
    SLIST_HEAD *pos    = NULL;

    free(device->path);
    device->path = NULL;

    // free(device->address);
    // device->address = NULL;

    SLIST_FOR_EACH_ENTRY(service, service_t, pos, &device->services, node)
    {
        remove_service(service);
    }

    if (device->connected) {
        ble_connected_device_num--;
    }

    free(device);
    device = NULL;
}

STATIC INT_T on_bt_iface_remove(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    device_t *device   = NULL;
    chrc_t *chrc       = NULL;
    service_t *service = NULL;
    SLIST_HEAD *pos    = NULL;
    CONST CHAR_T *obj  = NULL;
    INT_T op_ret;
    BOOL_T is_found = FALSE;

    IF_FAIL_RETURN(sd_bus_message_has_signature(m, "oas"));
    IF_FAIL_RETURN(sd_bus_message_read(m, "o", &obj));

    // 寻找设备挂载点
    tkl_mutex_lock(ble_devices_mutex);
    SLIST_FOR_EACH_ENTRY(device, device_t, pos, &ble_devices, node)
    {
        if (strcmp(device->path, obj) == 0) {
            tuya_slist_del(&ble_devices, &device->node);
            remove_device(device);
            tkl_mutex_unlock(ble_devices_mutex);

            return 0;
        }

        if (strstr(obj, device->path)) {
            is_found = TRUE;
            break;
        }
    }

    if (FALSE == is_found) {
        return 0;
    }
    is_found = FALSE;

    // 寻找服务挂载点
    SLIST_FOR_EACH_ENTRY(service, service_t, pos, &device->services, node)
    {
        if (strcmp(service->path, obj) == 0) {
            tuya_slist_del(&device->services, &service->node);
            remove_service(service);
            tkl_mutex_unlock(ble_devices_mutex);

            return 0;
        }

        if (strstr(obj, service->path)) {
            is_found = TRUE;
            break;
        }
    }

    if (FALSE == is_found) {
        return 0;
    }

    // 寻找chrc挂载点
    SLIST_FOR_EACH_ENTRY(chrc, chrc_t, pos, &service->chrcs, node)
    {
        if (strcmp(chrc->path, obj) == 0) {
            tuya_slist_del(&service->chrcs, &chrc->node);
            remove_chrc(chrc);
            tkl_mutex_unlock(ble_devices_mutex);

            return 0;
        }
    }
    tkl_mutex_unlock(ble_devices_mutex);

    return -2;
}

STATIC INT_T app_register_reply(sd_bus_message *m, VOID_T *userdata, sd_bus_error *ret_error)
{
    if (sd_bus_error_is_set(ret_error)) {
        printf("Error registering: %s: %s\n", ret_error->name, ret_error->message);
    }

    return 0;
}

STATIC INT_T gatt_init(sd_bus *bus)
{
    INT_T op_ret = OPRT_OK;

    IF_FAIL_RETURN(tkl_mutex_create_init(&ble_devices_mutex));
    IF_FAIL_RETURN_INFO(sd_bus_add_object_manager(bus, NULL, MAN_PATH), "BLE: Error adding object manager\n");

    IF_FAIL_RETURN(add_already_known_device(bus));

    IF_FAIL_RETURN_INFO(sd_bus_match_signal(bus, NULL, BUS_NAME, MAN_PATH, OBJECT_MANAGER_IFACE,
                                            "InterfacesAdded", on_bt_iface_add, bus),
                        "BLE: Error registering on_bt_iface_add\n");

    IF_FAIL_RETURN_INFO(sd_bus_match_signal(bus, NULL, BUS_NAME, MAN_PATH, OBJECT_MANAGER_IFACE,
                                            "InterfacesRemoved", on_bt_iface_remove, bus),
                        "BLE: Error registering on_bt_iface_remove\n");

    return op_ret;
}

STATIC VOID_T *bus_task(VOID_T *arg)
{
    INT_T r;

    while ((r = sd_bus_wait(bus, (uint64_t)-1)) >= 0) {
        while ((r = sd_bus_process(bus, NULL)) > 0) {
            continue;
        }

        if (r < 0) {
            printf("BLE: Error processing bus: %d\n", -r);
            return NULL;
        }
    }

    return NULL;
}

OPERATE_RET sd_bus_loop_start(VOID_T)
{
    if (0 != pthread_create(&bus_task_thId, NULL, bus_task, NULL)) {
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET sd_bus_init(VOID_T)
{
    INT_T r;

    if ((r = sd_bus_default_system(&bus)) < 0) {
        bus = NULL;
        printf("BLE: Get system bus err:%d\n", r);
        return OPRT_OS_ADAPTER_COM_ERROR;
    }
    gatt_init(bus);
    sd_bus_loop_start();

    return OPRT_OK;
}

OPERATE_RET sd_bus_deinit(VOID_T)
{
    sd_bus_unref(bus);

    return OPRT_OK;
}

OPERATE_RET sd_bus_gatts_service_add(TKL_BLE_GATTS_PARAMS_T *p_service_para)
{
    INT_T op_ret               = OPRT_OK;
    local_service_t *p_service = NULL;
    local_chrc_t *p_chrc       = NULL;

    if (is_service_add == TRUE) {
        return OPRT_OK;
    }

    if (bus == NULL) {
        sd_bus_init();
    }

    if (p_service_para->svc_num > LOCAL_SERVICES_NUM) {
        return OPRT_OS_ADAPTER_INVALID_PARM;
    }

    for (INT_T i = 0; i < p_service_para->svc_num; i++) {
        p_service = &(local_services[i]);
        memset(p_service->path, 0x00, SERVICE_CHRC_PATH_MAX_LENGTH);
        sprintf(p_service->path, "/org/bluez/service%d", i);
        memcpy(&(p_service->svc_uuid), &(p_service_para->p_service[i].svc_uuid), sizeof(TKL_BLE_UUID_T));
        p_service->type   = p_service_para->p_service[i].type;
        p_service->handle = 0;
        IF_FAIL_RETURN(sd_bus_add_object_vtable(bus, NULL, p_service->path, SERVICE_INTERFACE, svc_vtable, p_service));

        for (INT_T j = 0; j < p_service_para->p_service[i].char_num; j++) {
            p_chrc = &(p_service->chrcs[j]);
            memset(p_chrc->svc_path, 0x00, SERVICE_CHRC_PATH_MAX_LENGTH);
            memcpy(p_chrc->svc_path, p_service->path, SERVICE_CHRC_PATH_MAX_LENGTH);
            memset(p_chrc->path, 0x00, SERVICE_CHRC_PATH_MAX_LENGTH);
            sprintf(p_chrc->path, "%s/chrc%d", p_service->path, j);
            memset(p_chrc->data, 0x00, 512);
            p_chrc->data_len     = 0;
            p_chrc->para         = &(p_service_para->p_service[i].p_char[j]);
            p_chrc->para->handle = 0;
            IF_FAIL_RETURN(sd_bus_add_object_vtable(bus, NULL, p_chrc->path, CHRC_INTERFACE, chrc_vtable, p_chrc));
        }
    }

    IF_FAIL_RETURN_INFO(sd_bus_call_method_async(bus, NULL, BUS_NAME, BLUEZ_DEVICE_PATH,
                                                 GATT_MANAGER_IFACE, "RegisterApplication",
                                                 app_register_reply, NULL, "oa{sv}", MAN_PATH, 0),
                        "BLE: Error call method RegisterApplication\n");

    return OPRT_OK;
}

OPERATE_RET sd_bus_gatts_value_set(USHORT_T conn_handle, USHORT_T char_handle, UCHAR_T *p_data, USHORT_T length)
{
    local_service_t *p_service = NULL;
    local_chrc_t *p_chrc       = NULL;

    for (INT_T i = 0; i < LOCAL_SERVICES_NUM; i++) {
        p_service = &(local_services[i]);
        for (INT_T j = 0; j < LOCAL_CHRCS_NUM; j++) {
            p_chrc = &(p_service->chrcs[j]);
            if (p_chrc->para->handle == char_handle) {
                memcpy(p_chrc->data, p_data, length);
                p_chrc->data_len = length;

                return OPRT_OK;
            }
        }
    }
}

OPERATE_RET sd_bus_gatts_value_notify(USHORT_T conn_handle, USHORT_T char_handle, UCHAR_T *p_data, USHORT_T length)
{
    local_service_t *p_service = NULL;
    local_chrc_t *p_chrc       = NULL;

    for (INT_T i = 0; i < LOCAL_SERVICES_NUM; i++) {
        p_service = &(local_services[i]);
        for (INT_T j = 0; j < LOCAL_CHRCS_NUM; j++) {
            p_chrc = &(p_service->chrcs[j]);
            if (p_chrc->para->handle == char_handle) {
                if (length > 512) {
                    length = 512;
                }
                memcpy(p_chrc->data, p_data, length);
                p_chrc->data_len = length;
                if (sd_bus_emit_properties_changed(bus, p_chrc->path, CHRC_INTERFACE, "Value", NULL) < 0) {
                    printf("BLE: notify_send failed\n");
                    return OPRT_OS_ADAPTER_BLE_NOTIFY_FAILED;
                }

                return OPRT_OK;
            }
        }
    }

    return OPRT_OS_ADAPTER_NOT_SUPPORTED;
}

OPERATE_RET sd_bus_gatt_callback_register(CONST TKL_BLE_GATT_EVT_FUNC_CB gatt_evt)
{
    g_gatt_evt_cb = gatt_evt;

    return OPRT_OK;
}
