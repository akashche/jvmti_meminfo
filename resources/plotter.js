#!/usr/lib/jvm/java-1.8.0/bin/jjs

var files = Packages.java.nio.file.Files;
var paths = Packages.java.nio.file.Paths;
var system = Packages.java.lang.System;
var jstr = Packages.java.lang.String;

if (1 != arguments.length) {
    print("Error: invalid arguments");
    print("Usage: jjs plotter.js memlog.json");
    system.exit(1);
}
        
var binary = files.readAllBytes(paths.get(arguments[0]));
var json = String(new jstr(binary, "UTF-8"));
var list = JSON.parse(json);
var out = "";
for (var i = 0; i < list.length; i++) {
    out += ((i + 1) + " " + Math.round(list[i].os.overall/(1<<20)) + " " + 
            Math.round(list[i].jvm.overall/(1<<20)) + "\n");
}
files.write(paths.get("memlog.dat"), out.getBytes("UTF-8"));
print("Gnuplot data file written: memlog.dat");
system.exit(0);
