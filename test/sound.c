void busydelay(int c)
{
    while (c > 0) { c = c - 1; }
    $0xFFFFFC34 = 0;
}
void playcommon(int param, int notedelay)
{
    $0xFFFFFC30 = param;
    $0xFFFFFC34 = 1;
    busydelay(notedelay);
}

void playnote(int param)
{
    playcommon(param, 60000);
}

void playlongnote(int param)
{
    playcommon(param, 120000);
}

void playshortnote(int param)
{
    playcommon(param, 30000);
}

void main(void)
{
    playnote(0x07770EEE);
    playnote(0x06A60D4D);
    playnote(0x05EA0BDA);
    playnote(0x07770EEE);

    playnote(0x07770EEE);
    playnote(0x06A60D4D);
    playnote(0x05EA0BDA);
    playnote(0x07770EEE);

    playnote(0x05EA0BDA);
    playnote(0x05980B30);
    playlongnote(0x04FB09F7);

    playnote(0x05EA0BDA);
    playnote(0x05980B30);
    playlongnote(0x04FB09F7);

    playshortnote(0x04FB09F7);
    playshortnote(0x047108E1);
    playshortnote(0x04FB09F7);
    playshortnote(0x05980B30);
    playnote(0x05EA0BDA);
    playnote(0x07770EEE);

    playshortnote(0x04FB09F7);
    playshortnote(0x047108E1);
    playshortnote(0x04FB09F7);
    playshortnote(0x05980B30);
    playnote(0x05EA0BDA);
    playnote(0x07770EEE);

    playnote(0x07770EEE);
    playnote(0x09F713EE);
    playlongnote(0x07770EEE);

    playnote(0x07770EEE);
    playnote(0x09F713EE);
    playlongnote(0x07770EEE);
}
