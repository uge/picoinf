let sensor = new BMP280(0x77)

Log(`sensor.GetTemperatureCelsius()    = ${sensor.GetTemperatureCelsius()}`)
Log(`sensor.GetTemperatureFahrenheit() = ${sensor.GetTemperatureFahrenheit()}`)
Log(`sensor.GetPressureHectopascals()  = ${sensor.GetPressureHectopascals()}`)
Log(`sensor.GetPressureBars()          = ${sensor.GetPressureBars()}`)
Log(`sensor.GetAltitudeMeters()        = ${sensor.GetAltitudeMeters()}`)
Log(`sensor.GetAltitudeFeet()          = ${sensor.GetAltitudeFeet()}`)