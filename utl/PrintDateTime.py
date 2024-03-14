from datetime import datetime

now = datetime.now()
dt_string = now.strftime("%Y-%m-%d %H:%M:%S")
print(dt_string)
