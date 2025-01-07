let sensor = new BH1750(0x23)

sensor.SetTemperatureCelsius(20);
sensor.SetTemperatureFahrenheit(68);
Log(`sensor.GetLuxLowRes()   = ${sensor.GetLuxLowRes()}`);
Log(`sensor.GetLuxHighRes()  = ${sensor.GetLuxHighRes()}`);
Log(`sensor.GetLuxHigh2Res() = ${sensor.GetLuxHigh2Res()}`);