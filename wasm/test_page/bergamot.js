var worker;

if (window.Worker) {
    var worker = new Worker('worker.js');
    worker.postMessage(["load_module"]);
}

const log = (message) => {
    document.querySelector("#log").value += message + "\n";
}

document.querySelector("#translate").addEventListener("click", () => {
    translateCall();
});

document.querySelector("#from").addEventListener('keyup', function(event) {
    if (event.keyCode === 13) {
        translateCall();
    }
});

document.querySelector("#load").addEventListener("click", async() => {
    document.querySelector("#load").disabled = true;
    const lang = document.querySelector('input[name="modellang"]:checked').value;
    const from = lang.substring(0, 2);
    const to = lang.substring(2, 4);
    let start = Date.now();
    worker.postMessage(["load_model", from, to]);
    document.querySelector("#load").disabled = false;
});

const translateCall = () => {
    const text = document.querySelector('#from').value;
    const paragraphs = text.split("\n");

    worker.postMessage(["translate", paragraphs]);
}

worker.onmessage = function(e) {
    console.debug(`Message received from worker`);
    if (e.data[0] === 'translated_result') {
        document.querySelector('#to').value = e.data[1].join("\n");
        log(e.data[2]);
    }
    if ((e.data[0] === 'module_loaded') || (e.data[0] === 'model_loaded')) {
        log(e.data[1]);
    }
}