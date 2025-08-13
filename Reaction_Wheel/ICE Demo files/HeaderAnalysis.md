# CubeSpace CW0057 Reaction Wheel
Reaction wheels (RW) are satellite components that adjust orientation. By spinning the RW, the satellite spins in the opposite direction, allowing the satellite to be precisely oriented by controlling RW that are mounted on different axes.

The CubeSpace CW0057 RW vendor documentation indicates that this device runs with bare-metal firmware consisting of a bootloader and a control program.

Our job is to figure out how this device works so that we can implement our own rootkit.

Needed items:
- Linux tools:
  - vbindiff -> compare files
  - crc32 -> generate crc32 values
- Hex Editor -> analyze/modify files
- bootloader [.hex / .cs] -> vendor provided firmware
- controlprog [.hex / .cs] -> vendor provided firmware

### Linux Setup
<details closed>
<summary>Linux Tools Setup</summary>
To install Vbindiff, open up a terminal and run the following:

```bash
sudo apt install vbindiff
```
To install crc32, open up a terminal and run the following:
```bash
sudo apt install libarchive-zip-perl
```
</details>

## File Analysis Instructions
We start this process by gathering as much information as we can from the vendor documentation. Everything they tell us is something we don't have to discover for ourselves.

1. According to the vendor, the device runs on bare metal. What does this tell us about the firmware?
   <details closed>
   <summary>Hint</summary>
   There is no space on this embedded system to host an OS, so we can't expect code that must be interpreted by an OS.
   </details>
   <br>
   <details closed>
   <summary>Answer</summary>
   We expect the firmware to be a raw binary file (.bin) -> This allows it to run 'as-is' on a processor which is typical for bare metal applications.
   </details>
<br>

2. We have been given access to a .hex file and a .cs file for the bootloader and control program. The hex file is a relatively common format, but the .cs file is a format created by the vendor. Based on our analysis above, we expect the RW to need a .bin file, so what would be a logical guess for the design of the .cs file?
    <details closed>
    <summary>Answer</summary>
    The .cs file is most likely a .bin file -> it may or may not be modified in some way.
    </details>
    <br>

3. How can we test our assumption about the .cs file?
    <details closed>
    <summary>Hint</summary>
    The .hex format is commonly used as an intermediate step for compiling/flashing instructions onto bare metal devices.
    </details>
    <br>
    <details closed>
    <summary>Answer</summary>
    We can convert the .hex to .bin and then compare our .bin to the .cs file.
    </details>
    <br>

4. Open a terminal in the same location as your .hex file. Run the following command to generate a .bin from the .hex file:
   ```bash
   objcopy --input-target=ihex --output-target=binary controlprog.hex cpfromhex.bin
   ```
<br>

5. Now we want to compare the files side-by-side in our hex editor. Look for patterns, what do you notice?
   <details closed>
   <summary>Hint</summary>
   Look near byte 0x70 in the .cs
   </details>
   <br>
   <details closed>
   <summary>Answer</summary>
   Start of .bin:
   <br>
   <img width="740" height="110" alt="image" src="/img/fromHexClip.PNG" />
   <br>
   Byte 73 of .cs:
   <br>
   <img width="740" height="282" alt="image" src="/img/csClip.PNG" />
   </details>
<br>

6. It appears that the .cs is a .bin file with a header. We can confirm this by removing the header and saving it separately.

   Highlight the first 0x72 bytes in your hex editor and delete them -> save the file as 'cptrim.bin'.

   **Note: If you use a VS Code extension as your hex editor this may not work the first time. Close out VS code, re-open controlprog.cs and repeat the process. Sometimes VS Code corrupts the new file while saving it as a .bin.**

   Repeat the process, but this time save only the header as 'cphead.bin'.

7. Now that we have our trimmed file, we can use vbindiff to verify if we have any differences between our cpfromhex.bin and our cptrim.bin. Open a terminal in the same folder as your files and run the following:

   ``` bash
   vbindiff cptrim.bin cpfromhex.bin
   ```
   What do you see?
   <details closed>
    <summary>Answer</summary>
    The files are the same. Pressing RET to find next difference goes to the end of the file.
    <br>
    <img width="959" height="990" alt="image" src="/img/vbindiff.PNG" />
    <br>
    We have now proven that our .cs is a .bin file with a header. Next we'll analyze the header.
   </details>

## .CS Header Analysis
Before we dive in to the header analysis, we should consider what information we expect to find.

1. Discussion Questions:
   <details closed>
    <summary>What do we already know about the operation of this device?</summary>
    It runs bare metal code for a processor
   </details>
   <details closed>
    <summary>How does a processor interpret instructions?</summary>
    It reads commands sequentially, editing data and following jumps as they're encountered -> It doesn't 'think' big-picture, it only understands the current instruction.
   </details>
   <details closed>
    <summary>Would this file work if it were stored incorrectly?</summary>
    No -> If the file is not located precisely where it should be in memory, the processor will not run the program correctly.
   </details>
   <br>
2. Limited by the constraints of bare metal programming, what kind of questions does the header need to answer to successfully load this program?
   <details closed>
    <summary>Hint</summary>
    The bootloader itself is running on bare metal and is responsible for receiving and loading the control program.
   </details>
   <br>
   <details closed>
    <summary>Answer</summary>
    - How big is this file? <br>
    - How do I know I have the right file? <br>
    - Where does this file go? <br>
    - What other information could be useful for the creator to save? <br>
   </details>
   <br>
3. Let's start looking at the header: The very first byte is 0x72.
   <details closed>
    <summary>Have we seen this number before? Where?</summary>
    When we trimmed the file.
   </details>
   <details closed>
    <summary>What do we think this field is?</summary>
    Header length.
   </details>
   <details closed>
    <summary>How big is this field? Do people prefer odd or even numbers?</summary>
    It is most likely not a single-byte field, probably 2 bytes given that it's a smaller number.
   </details>
   <details closed>
    <summary>What does the layout of this field tell us?</summary>
    Assuming a 2x byte field -> this data is stored little-endian.
   </details>
   <br>
4. This header is for a device from a company that manufactures a wide range of space components. If your main focus is making money, one of your key goals is to minimize costs. Reusing code is very good for saving time/money -> how would you develop a header if you wanted to use it across many different devices, including some that aren't designed yet?

   <details closed>
    <summary>Answer</summary>
    Use Padding. You can reserve space for future growth while also fixing locations for data fields.
    <br> What data value does CubeSpace use for padding? -> 00
   </details>
   <br>

5. Moving on to the next data bytes, we see 70 03 03:
   <details closed>
    <summary>Do we think this is the whole field?</summary>
     People like even numbers -> We probably need to include the trailing 00 in this field.
   </details>
   <details closed>
    <summary>How should we read this number?</summary>
     Assuming little-endian -> 00030370.
   </details>
   <details closed>
    <summary>Have we seen this number? Based on what we've already seen, what's a good guess for this field?</summary>
    We've already seen a header length, it would make sense to have a data length field.
   </details>
   <details closed>
    <summary>How can we check this assumption?</summary>
    Refer to vbindiff screenshot above, the final line of the cptrim file is 00030370. (The first byte is located at 00000000, so the file is this long even though the last data byte is at address 0003036f).
   </details>
   <br>

6. We have now found two length fields. The vendor documentation briefly describes that there is a check for file integrity and size as a safety measure to prevent overflowing the processor memory with an invalid control program.

   How would we ensure file integrity?
   
   <details closed>
    <summary>Hint</summary>
    There are multiple possible methods, so 'guess and check' is a valid technique for trying them all -> however, we already told you above with the 'needed tools'.
   </details>
   <details closed>
    <summary>Answer</summary>
    Cyclic Redundancy Check (CRC). We won't cover the details of how it works here, but it is a technique for checking if a file was transmitted/stored correctly.
   </details>
   <br>
   <details closed>
    <summary>Is this a valid method of data security?</summary>
    NO! We will exploit this weakness later in the exercise.
   </details>
   <br>
   Let's figure out what value we should be looking for -> what file should we focus on?
   <details closed>
    <summary>Hint</summary>
    We need to find a crc, but is the header necessary for the program to run?
   </details>
   <details closed>
   <summary>Answer</summary>
    The header is only used for helping the bootloader load the program into the processor memory -> we need to find the crc for our cptrim.bin file.
   </details>
   <br>
   Open up a terminal and run the following:

   ```bash
   crc32 'filename'
   ```
   What is our output?
   <details closed>
   <summary>Answer</summary>
    <img width="735" height="85" alt="71AD7106" src="/img/crc.PNG" />
   </details>
<br>

7. Based on what we've seen so far, it's reasonable to expect even-byte little-endian data fields for the rest of our header. Looking at the next 4 bytes, we see 00 40 01 08 -> 08014000.
   <details closed>
   <summary>Do we see this number anywhere?</summary>
   No. It's too big to be a location in our program -> time to step away from the code files for a minute.
   </details>
   <br>
   We still haven't confirmed anything about the hardware on the reaction wheel -> we will do that now.
   <details closed>
   <summary>Open the Case</summary>
   We see the following:
   <br>
   <img width="800" height="765" alt="image" src="/img/RWInternal.png" />   
   </details>
   <details closed>
   <summary>What kind of processor do we have?</summary>
   It's an STM32L452CEU3.
   </details>
   <details closed>
   <summary>What should we do now?</summary>
   Google it and find a data sheet!

   Looking it up we learn that it is an ARM processor with 512kB of flash memory.

   <img width="537" height="347" alt="image" src="/img/STMlookup.png" />

   Our data sheet also conveniently contains a memory map for the processor:

   <img width="830" height="380" alt="image" src="/img/STMmemory.png" />
   </details>
   <details closed>
   <summary>Given everything we just learned, what is that data field?</summary>
   It's a write address, telling the bootloader where to save this program in the processor memory.
   </details>
<br>

8. We've found some critical data fields that we will need later, can you identify the next few fields in the header? What are the values?
   <details closed>
   <summary>Hint</summary>
   We wouldn't expect endianness to change, and people typically like even numbers when working with binary data.
   </details>
   <details>
   <summary>Answer</summary>
   Assuming 4 byte fields, the next three fields contain the data 0x03, 0x01, and 0x30. These fields are followed by some padding identied by multiple bytes of 00.
   </details>
   <details>
   <summary>Do we know what these fields are?</summary>
   It's possible that these are a version number or some other kind of internal 'note' for the person that created the header. We don't have any conclusive evidence of these field identities.
   </details>
   <br>

9. Next, we finally encounter some bytes that make sense as a string!
   <details>
   <summary>What does the first string tell us?</summary>
   This string identifies the intended device that the program is meant to run on. The cube wheel is the vendor's name for a reaction wheel.
   </details>
   <details>
   <summary>What does the second string tell us?</summary>
   This string identifies the file type. This file is a control program.
   </details>
   <details>
   <summary>What is the length of these data fields?</summary>
   There are 32 bytes between the start of each string and the start of the first data following the strings -> It's reasonable to assume each string is either a 16 byte string followed by 16 bytes of padding, or a 32 byte string, we can't know for sure.
   </details>
   <details>
   <summary>Why include these fields?</summary>
   This is speculation -> The vendor is likely utilizing a common header for all firmware on all devices they make. These data fields allow for a programmer to quickly ID what file they have open. The fact that these strings are stored in a readable layout (instead of little-endian) supports the idea that these data fields are primarily meant for people.
   </details>
   <br>

10. Following our strings, we have a few more bytes to interpret. We mentioned earlier that it's best to gather as much information from the vendor as possible to save time. To that end, we connected our RW to the vendor software and started clicking around and stumbled across build numbers and version numbers that match these bytes -> This led us to the conclusion that the 8 bytes following the strings are build/version info.
    <details>
    <summary>Do we actually care about that?</summary>
    Not necessarily -> but identifying these data fields as the build/version numbers means that those earlier values of 0x03, 0x01, and 0x30 are something else.
    </details>
    <br>

11. Finally, we have 4 bytes remaining in our header.
<br> Is there any typical data for file creation that is missing?
    <details>
    <summary>Hint</summary>
    Every piece of work you have ever turned in (homework, job, etc) usually includes a few key pieces of information at the top of the first page. What have we not seen in this header yet?
    </details>
    <details>
    <summary>Answer</summary>
    A date. Software commonly has a 'build time' that can be stored in the compiled file. This time is generally given in seconds from a starting point: often the Unix Time Epoch of January 1st, 1970.
    </details>
    <details>
    <summary>How do we check this assumption?</summary>
    Note: Online converters make this easy. <br>
    First, we convert our data (66C884D2) into a decimal value: 1724417234
    <br>
    <img width="870" height="450" alt="image" src="/img/buildtime.PNG" />
    <br>
    Then we plug this value into an epoch converter:
    <img width="800" height="290" alt="image" src="/img/date.PNG" />
    </details>

## .CS Header Summary
We have finished analyzing our header and have identified several key fields that we will use later. For convenience, here is a summary image showing the fields we identified.
<details>
   <summary>Header Fields</summary>
   <img width="904" height="532" alt="image" src="/img/headerMap.png" />
   </details>

## Bootloader Header Analysis
Now it's your turn! Answer the following questions using everything we've discussed previously along with the bootloader files:

1. How big is the bootloader header?
   <details>
   <summary>Answer</summary>
   0x72 bytes
   </details>
   <details>
   <summary>What can we infer about the header from this data value?</summary>
   It is probably identical to the header format we just analyzed.
   </details>
   <br>
2. How big is the bootloader file and where is it stored on our processor?
   <details>
   <summary>Answer</summary>
   It is 0x00011938 bytes, stored at 0x08000000
   </details>
   <details>
   <summary>What is noteworthy about this address?</summary>
   It is the start of flash memory on the processor.
   </details>
   <br>
3. What's the CRC for the bootloader file?
   <details>
   <summary>Answer</summary>
   B093A03E
   </details>
   <br>
4. Using the bootloader file, how can we update our analysis of the string fields?
   <details>
   <summary>Answer</summary>
   These fields contain strings greater than 16 bytes -> it makes sense to conclude the string fields are 32 bytes.
   </details>
   <br>
5. Was the bootloader or the control program compiled first? (Don't use a converter for this task)
   <details>
   <summary>Answer</summary>
   These fields are identical between files except for the last byte (C0 vs D2) -> C0 is smaller than D2 -> The bootloader was compiled first, by 18 seconds.
   </details>
   <br>
6. CHALLENGE: Assuming nothing else is stored besides the bootloader and control program, what does the flash memory look like on the processor? How much total open space is available?
   <details>
   <summary>Hint</summary>
   The numbers we see in these headers are hex values, not decimal -> don't forget to convert when discussing available space. Are the bootloader and control program back-to-back in memory?
   </details>
   <details>
   <summary>Answer</summary>
   
   Bootloader: 0x08000000 -> 0x08011937<br>
   Empty: 0x08011938 -> 0x08013FFF<br>
   Control Program: 0x08014000 -> 0x0804436F<br>
   Empty: 0x08044370 -> 0x0807FFFF (reserved memory starts 0x08080000)<br>

   Available Memory:<br>
   Empty1 = 3FFF - 1938 (+1) = 26C8 = 9928 bytes<br>
   Empty2 = 7FFFF - 44370 (+1) = 3BC90 = 244880 bytes<br>
   Total = 9928 + 244880 = 254,808 bytes ~ 255 kB
   </details>
   <details>
   <summary>Follow Up Question</summary>
   Why would the bootloader and control program not be stored back-to-back?<br> -> Allows files to be updated individually. If the bootloader got 1 byte bigger, and the files were stored back-to-back, you'd have to reconfigure the entire control program too.
   </details>