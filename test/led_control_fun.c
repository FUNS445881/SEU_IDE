void delay(void)
{
int c;c=30000;
while(c>0){ c=c-1;}
}
void main(void)
{
    int x; x = 1;
    while (1)
    {
        $0xFFFFFC60 = x;
        x = x << 1;
        if (x == 0x10000)
            x = 1;
        delay();
    }
}
