// import modules
let eventLoop = require("event_loop");
let gui = require("gui");
let submenuView = require("gui/submenu");
let notify = require("notification");
let textInputView = require("gui/text_input");
let dialogView = require("gui/dialog");
let flipper = require("flipper");
let badusb = require("badusb");
let currentWord = "Hello World";
let speed = 2;
let delayArr = [
    [0,0,0,0],
    [4000,3500,2000,2500,60],
    [2500,2500,1500,2000,30],
    [1500, 2000, 1000,1500,0]
]; //this delay array allows users to change the speed that the program runs

let views = {
    keyboard: textInputView.makeWith({
        header: "Enter word:",
        minLength: 0,
        maxLength: 30,
        defaultText: currentWord,
        defaultTextClear: true,
    }),
    startDialog: dialogView.make(),
    setSpeedKeyboard: textInputView.makeWith({
            header: "Set speed:(1,2,3)",
            minLength: 1,
            maxLength: 1,
            defaultText: speed.toString(),
            defaultTextClear: true,
    }),
    doneSetWordDialog: dialogView.make(),
    menu: submenuView.makeWith({
        header: "Chrome Badusb App",
        items: [
            "Set word",
            "Set speed",
            "Start",
        ],
    }),
};

badusb.setup({
    vid: 0xAAAA,
    pid: 0xBBBB,
    mfrName: "Flipper",
    prodName: "Zero",
    layoutPath: "/ext/badusb/assets/layouts/en-US.kl"
});
eventLoop.subscribe(views.menu.chosen, function (_sub, index, gui, eventLoop, views) {
    if (index === 0) {
        gui.viewDispatcher.switchTo(views.keyboard);
    }else if(index === 2){
        views.startDialog.set("text", "Chrome Typer");
        views.startDialog.set("center","Start");
        gui.viewDispatcher.switchTo(views.startDialog);
    }else if(index === 1){
        gui.viewDispatcher.switchTo(views.setSpeedKeyboard);
    }
}, gui, eventLoop, views);


    eventLoop.subscribe(views.setSpeedKeyboard.input, function(_ub, word, gui, views) {
        speed = parseInt(word, 10); 
    });
    

eventLoop.subscribe(views.keyboard.input, function(_sub, word, gui, views){
    views.doneSetWordDialog.set("text","Word is now " + word);
    views.doneSetWordDialog.set("center","Go back");
    currentWord = word;
    gui.viewDispatcher.switchTo(views.doneSetWordDialog);
},gui,views);


eventLoop.subscribe(views.doneSetWordDialog.input, function (_sub, button, gui, views) {
    if (button === "center")
        gui.viewDispatcher.switchTo(views.menu);
}, gui, views);


eventLoop.subscribe(gui.viewDispatcher.navigation, function (_sub, _, gui, views, eventLoop) {
    if (gui.viewDispatcher.currentView === views.menu) {
        eventLoop.stop();
        return;
    }
    gui.viewDispatcher.switchTo(views.menu);
}, gui, views, eventLoop);

eventLoop.subscribe(views.startDialog.input, function (_sub, button, gui, views) {
    if (badusb.isConnected()) {
        if (currentWord === undefined) {
            views.startDialog.set("text", "No word selected");
            views.startDialog.set("center","");
        }
        notify.blink("green", "short");
        views.startDialog.set("text", "Running...");
        views.startDialog.set("center","");
        badusb.press("GUI","r");
        delay(delayArr[speed][0]);
        badusb.print("chrome");
        badusb.press("ENTER");
        delay(delayArr[speed][1]);
        badusb.press("TAB");
        badusb.press("ENTER");
        delay(delayArr[speed][2]);
        badusb.press("CTRL","t");
        delay(delayArr[speed][3]);
        badusb.print(currentWord);
        delay(delayArr[speed][4]);
        badusb.press("ENTER");
        notify.success();
        views.startDialog.set("text", "Finished");
        views.startDialog.set("center","Redo");
    } else {
        print("USB not connected");
        notify.error();
    }
    return;
    
}, gui, views);



gui.viewDispatcher.switchTo(views.menu);
eventLoop.run();
