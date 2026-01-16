void main(void)
{
    int x; 
    int c；
    x = 1;
    while (1)
    {
        c = 3000000;
        $0xFFFFFC60 = x;
        x = x << 1;
        if (x == 0x10000)
            x = 1;
        while(c>0){ c=c-1;}
    }
}
