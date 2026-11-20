import csv
import subprocess

out = subprocess.check_output(["./sensorlog", "--demo"], text=True)
rows = list(csv.DictReader(out.splitlines()))
assert [int(r["elapsed_ms"]) for r in rows] == list(range(0, 8000, 1000))
assert [r["status"] for r in rows] == ["ok","ok","missing","missing","recovered","ok","ok","ok"]
assert [r["temperature_c"] for r in rows] == [
    "25.0000","25.0625","","","-1.0000","0.0000","0.0625","1.0000"]
assert subprocess.run(["./sensorlog","--i2c","/nonexistent/sensorlog-bus"]).returncode == 1
assert subprocess.run(["./sensorlog","--wrong"]).returncode == 2
print("PASS: timestamps, disconnection, recovery across batches, negative and fractional temperatures")
