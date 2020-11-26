# MyM5StickC

koudenpa's M5StickC.

## Function

### Send env to Mackerel

It reads the temperature and humidity from the environmental sensor and sends it to [Mackerel](https://mackerel.io/).

Sending to Mackerel uses the [library](https://github.com/7474/ArduinoMackerelClient) I'm making now.

### Stamp AKASHI

Press button A to [stamp AKASHI](https://akashi.zendesk.com/hc/ja/articles/115000475854-AKASHI-%E5%85%AC%E9%96%8BAPI-%E4%BB%95%E6%A7%98#stamp).

Uses the [library](https://github.com/7474/ArduinoAkashiClient) I'm making now.

## Target

- [M5StickC](https://www.switch-science.com/catalog/5517/)
- [M5StickC ENV Hat（DHT12/BMP280/BMM150搭載）](https://www.switch-science.com/catalog/5755/)

## Setup

Set environment variables.
```
cp env.h.sample env.h
# And edit it.
```

## Thanks

- https://github.com/closedcube/ClosedCube_SHT31D_Arduino
- https://github.com/adafruit/Adafruit_BMP280_Library
