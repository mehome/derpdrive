#!/usr/bin/ts-node

const fs = require('fs');

console.log(process.argv);

if (process.argv.length !== 5) {
    console.error('Usage: instructionparser.js <input> <output> <class>')
    return 1;
}

const infile = fs.readFileSync(process.argv[2]);
const outfile = fs.openSync(process.argv[3], 'w');
const classname = process.argv[4];

let table = [];

function BinaryToInt(binary) {
    let v = 0;
    for(let i=0; i < binary.length; i++) {
        v = v << 1;
        if (binary[i] === '1')
            v = v | 1;
    }

    return v;
}

// Read in table of instruction generators and names
console.log(`Reading ${process.argv[2]}...`);
let inputLine = '';
let line = 1;
for(let i=0; i < infile.length; i++) {
    if (infile[i] === 0x0A || i === infile.length-1) {
        let tokens = inputLine.split(/\s+/);
        
        if (tokens.length === 2 && tokens[0].length && tokens[1].length) {
            table.push({
                line,
                name: tokens[1],
                gen: tokens[0]
            });
        } else {
            console.error(`Input error on line ${line}: ${tokens.length} (${inputLine})`);
        }

        inputLine = '';
        line++;
        continue;
    } else if (infile[i] == 0x0D) {
        continue;
    }

    inputLine += String.fromCharCode(infile[i]);
}

// Compute the mask and signature values from the generator string
console.log('Computing mask and signature values...');
for(let i=0; i < table.length; i++) {
    let mask = '', signature = '';
    let directBitCount = 0;

    for(let s=0; s < table[i].gen.length; s++) {
        if (table[i].gen[s] == '0' ||
            table[i].gen[s] == '1') {

            directBitCount++;
            mask += '1';
            signature += table[i].gen[s];
        } else {
            mask += '0';
            signature += '0';
        }
    }

    table[i].distinctSize = directBitCount;
    table[i].mask = BinaryToInt(mask);
    table[i].signature = BinaryToInt(signature);
}

table.sort(function (a, b) {
    return b.distinctSize - a.distinctSize;
});

// Make sure all entries are distinct
console.log('Checking distinguishability...');
let distinguishabilityCheckFailed = false;
for(let i=0; i < table.length; i++) {
    for(let s=0; s < table.length; s++) {
        if(s === i) 
            continue;

        if (table[i].signature === table[s].signature && table[i].directBitCount != table[s].directBitCount) {
            console.error(`Error: Entry line ${table[i].line} (${table[i].gen}, ${table[i].name}) is not distinguishable`);
            console.error(`from entry line ${table[s].line} (${table[s].gen}, ${table[s].name})!`);
            distinguishabilityCheckFailed = true;
        }
    }
}

if (distinguishabilityCheckFailed) {
    return -1;
}

// Generate the instruction decode table
console.log(`Writing '${process.argv[3]}' file...`)
for(let i=0; i < table.length; i++) {
    if (i != table.length - 1) {
        fs.writeSync(outfile, 
            ` { 0x${table[i].mask.toString(16)}, 0x${table[i].signature.toString(16)}, &${classname}::Execute${table[i].name} },\n`);
    } else {
        fs.writeSync(outfile, 
            ` { 0x${table[i].mask.toString(16)}, 0x${table[i].signature.toString(16)}, &${classname}::Execute${table[i].name} }\n`);
    }
}

return 0;