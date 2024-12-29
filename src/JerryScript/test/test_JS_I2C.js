let i2c = new I2C(1, 'Doug');
i2c.print();

let i2c2 = new I2C(2, 'Doug2');
i2c2.print();

function Test(addr, name)
{
    let i2c = new I2C(addr, name);
    i2c.print();
}

Test(3, "3");
Test(4, "4");

I2C(13, "unlucky");