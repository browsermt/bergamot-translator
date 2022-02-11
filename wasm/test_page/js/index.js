let worker;
let modelRegistry;

const $ = selector => document.querySelector(selector);
const status = message => ($("#status").innerText = message);

const langFrom = $("#lang-from");
const langTo = $("#lang-to");

const langs = [
  ["en", "English"],
  ["it", "Italian"],
  ["pt", "Portuguese"],
  ["ru", "Russian"],
  ["cs", "Czech"],
  ["de", "German"],
  ["es", "Spanish"],
  ["et", "Estonian"],
];

if (window.Worker) {
  worker = new Worker("js/worker.js");
  worker.postMessage(["import"]);
}

document.querySelector("#input").addEventListener("keyup", function (event) {
  translateCall();
});

const _prepareTranslateOptions = (paragraphs) => {
  const translateOptions = [];
  paragraphs.forEach(paragraph => {
    // Each option object can be different for each entry. But to keep the test page simple,
    // we just keep all the options same (specifically avoiding parsing the input to determine
    // html/non-html text)
    translateOptions.push({"isQualityScores": true, "isHtml": true});
  });
  return translateOptions;
};

const translateCall = () => {
  const text = document.querySelector("#input").value + "  ";
  if (!text.trim().length) return;

  const paragraphs = text.split("\n");
  const translateOptions = _prepareTranslateOptions(paragraphs);
  $("#output").setAttribute("disabled", true);
  const lngFrom = langFrom.value;
  const lngTo = langTo.value;
  worker.postMessage(["translate", lngFrom, lngTo, paragraphs, translateOptions]);
};

worker.onmessage = function (e) {
  if (e.data[0] === "translate_reply" && e.data[1]) {
    document.querySelector("#output").value = e.data[1].join("\n\n");
    $("#output").removeAttribute("disabled");
  } else if (e.data[0] === "load_model_reply" && e.data[1]) {
    status(e.data[1]);
    translateCall();
  } else if (e.data[0] === "import_reply" && e.data[1]) {
    modelRegistry = e.data[1];
    init();
  }
};

langs.forEach(([code, name]) => {
  langFrom.innerHTML += `<option value="${code}">${name}</option>`;
  langTo.innerHTML += `<option value="${code}">${name}</option>`;
});

const loadModel = () => {
  const lngFrom = langFrom.value;
  const lngTo = langTo.value;
  if (lngFrom !== lngTo) {
    status(`Installing model...`);
    console.log(`Loading model '${lngFrom}${lngTo}'`);
    worker.postMessage(["load_model", lngFrom, lngTo]);
  } else {
    const input = document.querySelector("#input").value;
    document.querySelector("#output").value = input;
  }
};

langFrom.addEventListener("change", e => {
  loadModel();
});

langTo.addEventListener("change", e => {
  loadModel();
});

$(".swap").addEventListener("click", e => {
  [langFrom.value, langTo.value] = [langTo.value, langFrom.value];
  $("#input").value = $("#output").value;
  loadModel();
});

function init() {
  // try to guess input language from user agent
  let myLang = navigator.language;
  if (myLang) {
    myLang = myLang.split("-")[0];
    let langIndex = langs.findIndex(([code]) => code === myLang);
    if (langIndex > -1) {
      console.log("guessing input language is", myLang);
      langFrom.value = myLang;
    }
  }

  // find first output lang that *isn't* input language
  langTo.value = langs.find(([code]) => code !== langFrom.value)[0];
  // load this model
  loadModel();
}
