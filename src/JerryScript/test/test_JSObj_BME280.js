let sensor = new BME280(0x76)

Log(`sensor.GetTemperatureCelsius()    = ${sensor.GetTemperatureCelsius()}`)
Log(`sensor.GetTemperatureFahrenheit() = ${sensor.GetTemperatureFahrenheit()}`)
Log(`sensor.GetPressureHectoPascals()  = ${sensor.GetPressureHectoPascals()}`)
Log(`sensor.GetPressureMilliBars()     = ${sensor.GetPressureMilliBars()}`)
Log(`sensor.GetAltitudeMeters()        = ${sensor.GetAltitudeMeters()}`)
Log(`sensor.GetAltitudeFeet()          = ${sensor.GetAltitudeFeet()}`)
Log(`sensor.GetHumidityPct()           = ${sensor.GetHumidityPct()}`)