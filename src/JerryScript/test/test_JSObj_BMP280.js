let sensor = new BMP280(0x77)

Log(`sensor.GetTemperatureCelsius()    = ${sensor.GetTemperatureCelsius()}`)
Log(`sensor.GetTemperatureFahrenheit() = ${sensor.GetTemperatureFahrenheit()}`)
Log(`sensor.GetPressureHectoPascals()  = ${sensor.GetPressureHectoPascals()}`)
Log(`sensor.GetPressureMilliBars()     = ${sensor.GetPressureMilliBars()}`)
Log(`sensor.GetAltitudeMeters()        = ${sensor.GetAltitudeMeters()}`)
Log(`sensor.GetAltitudeFeet()          = ${sensor.GetAltitudeFeet()}`)