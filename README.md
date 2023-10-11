# Measurement station with Mayfly data logger and Atlas Scientific sensors
  
This arduino code is derived from [https://github.com/movingplaid/Mayfly_ContinuousTemperatureLogger](https://github.com/movingplaid/Mayfly_ContinuousTemperatureLogger).
It manages measurement station with multiple Atlas Scientific sensors that opeate in I2C mode and Mayfly board. Ezo_board class is used to manage Atlas Sensors and all attached sensors should be defined at the beginning. Reading of the sensors should be added to function createDataRecord(). The data is logged to SD card.




