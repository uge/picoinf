print(`${msg1}`)

for (const key in msg1)
{
    if (msg1.hasOwnProperty(key))
    {
        print(`msg1.${key} = ${typeof msg1[key]}`)
    }
}

print(`msg1.GetAltitudeFeet() = ${msg1.GetAltitudeFeet()}`);
msg1.SetAltitudeFeet(15);
print(`msg1.GetAltitudeFeet() = ${msg1.GetAltitudeFeet()}`);
msg1.SetAltitudeFeet(-15);
print(`msg1.GetAltitudeFeet() = ${msg1.GetAltitudeFeet()}`);
msg1.SetAltitudeFeet(25);
print(`msg1.GetAltitudeFeet() = ${msg1.GetAltitudeFeet()}`);
msg1.SetAltitudeFeet(12);
print(`msg1.GetAltitudeFeet() = ${msg1.GetAltitudeFeet()}`);