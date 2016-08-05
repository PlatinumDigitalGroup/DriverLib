# DriverLib
Makes drivers less sucky to manage from usermode.

## Example usage

    DriverLib::IDriver* driver = DriverLib::DriverFactory(L"Fonzyhuektheplanet").setFilePath(L"C:\\fonz64.sys")->setDeviceName(L"dbk64")->build();
        driver->load();
        driver->sendIOControlRequest<Request>(IOCTL_DEFINITION, new Request());
        driver->sendIOControlRequest<Request, Response>(IOCTL_DEFINITION, new Request(), new Response));
        driver->unload();
        delete driver;
