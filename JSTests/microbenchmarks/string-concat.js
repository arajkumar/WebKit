function test(a, b, c, d, e)
{
    return a.concat(b).concat(c).concat(d).concat(e);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test("Cocoa", "Cappuccino", "Matcha", "Rize", "Kilimanjaro");
