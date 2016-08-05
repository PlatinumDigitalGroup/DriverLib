# DriverLib
Makes drivers less sucky to manage from usermode.

## Example usage

There are two "types" of drivers - "generic" drivers (which are loaded from disk), and "buffered" drivers (which are loaded from a byte array).  To help manage this polymorphism, IDriver instances must be created with DriverFactory.  If a file path is specified, the IDriver instance will be a type of GenericDriver, and if a byte buffer is specified, the IDriver instance will be a type of BufferedDriver.

Here's a short GenericDriver example that loads the driver C:\fonz64.sys.

    DriverLib::IDriver* driver = DriverLib::DriverFactory(L"fonz64")
        .setFilePath(L"C:\\fonz64.sys")
        ->setDeviceName(L"dbk64")
        ->build();
    driver->load();
    driver->sendIOControlRequest<Request>(IOCTL_DEFINITION, new Request());
    driver->sendIOControlRequest<Request, Response>(IOCTL_DEFINITION, new Request(), new Response()));
    driver->unload();
    delete driver;

NB: IDriver does not clean up for you.  You must still unload the driver before deleting the driver object.
