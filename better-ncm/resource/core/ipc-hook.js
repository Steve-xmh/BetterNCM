// For IPC Call debug

const rawCalls = {}

for (const key in channel) {
    const rawCall = channel[key];
    rawCalls[key] = rawCall;

    channel[key] = function (...args) {
        if (localStorage.getItem('better-ncm-ipc-hook-enabled') !== null) {
            console.groupCollapsed('[IPCHook] channel.' + key)
            console.groupCollapsed('args')
            for (const arg of args) {
                console.log(arg);
            }
            console.groupEnd()
            const result = rawCall(...args);
            console.groupCollapsed('returns')
            console.log(result);
            console.groupEnd()
            console.groupEnd()
            return result
        } else {
            return rawCall(...args);
        }
    };
}

channel.encryptId = rawCalls.encryptId
channel.deSerialData = rawCalls.deSerialData
channel.deData = rawCalls.deData

window.commandSet = new Set()
window.registerCommandSet = new Set()

channel.registerCall = function (command, callback) {
    window.registerCommandSet.add(command)
    return rawCalls.registerCall(command, callback)
}

channel.call = function (command, callback, params) {
    if (localStorage.getItem('better-ncm-ipc-hook-enabled') === null) {
        const result = rawCalls.call(command, callback, params);
        return result
    }
    window.commandSet.add(command)
    function callbackWrapper(...args) {
        console.groupCollapsed('[IPCHook] channel.call', command)
        console.groupCollapsed('callback')
        console.log(callback)
        console.groupEnd()
        console.groupCollapsed('params')
        for (const param of params) {
            console.log(param)
        }
        console.groupEnd()
        console.groupCollapsed('returns')
        for (const arg of args) {
            console.log(arg)
        }
        console.groupEnd()
        console.groupEnd()
        // LEDMOZycpISDMTwkfCdXgLmExhliIaOrvqbSExwdowCFJrbIKpUclake
        // if (command.length == 56 && !ignoredCommands.includes(command)) {
        //     debugger;
        // }
        
        callback(...args)
    }
    // if (command.length == 56 && !ignoredCommands.includes(command)) {
    //     debugger;
    // }
    const result = rawCalls.call(command, callbackWrapper, params);
    return result
};

window.ipcCall = function(command, ...args) {
    return new Promise((resolve) => {
        channel.call(command, resolve, args)
    })
}
