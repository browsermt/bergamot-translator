let worker;
let modelRegistry;

let SERIAL = 0;

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
  worker.postMessage([0, "import"]);
}

const panelTemplate = document.querySelector("#output-panel-template");

document.body.addEventListener("click", e => {
  if (e.target.matches("#translate-btn")) {
    translateCall();
  } else if (e.target.matches(".close-btn")) {
    e.target.closest(".panel").remove();
  } else if (e.target.matches("#set-gtranslate-key")) {
    const key = prompt('Google Translate key', window.localStorage['GOOGLE_TRANSLATE_KEY']);
    if (key !== null) {
      window.localStorage['GOOGLE_TRANSLATE_KEY'] = key;
    }
  }
})

document.querySelector("#input").addEventListener("keypress", e => {
  if (e.key == "Enter" && e.shiftKey) {
    e.preventDefault();
    if (!document.querySelector("#translate-btn").disabled) {
      translateCall();
    }
  }
})

const translateCall = () => {
  const text = document.querySelector("#input").value;
  if (!text.trim().length) return;
  const id = SERIAL++;
  const lngFrom = langFrom.value;
  const lngTo = langTo.value;
  const options = {
    html: document.querySelector("#input-is-html").checked,
    voidTags: document.querySelector("#html-void-tags").value,
    inlineTags: document.querySelector("#html-inline-tags").value,
    continuationDelimiters: document.querySelector("#html-continuation-delimiters").value,
  };

  worker.postMessage([id, "translate", lngFrom, lngTo, text, options]);

  const panel = panelTemplate.content.cloneNode(true);
  panel.querySelector('.panel').id = `translation-output-${id}`;
  panel.querySelector('.panel').classList.add('loading');
  panel.querySelector('.panel .input .html').srcdoc = text;
  panel.querySelector('.panel .input .raw').innerText = addControlCharacters(text);
  
  const outputs = document.querySelector('#outputs');
  outputs.insertBefore(panel, outputs.firstChild);

  googleTranslate(id, lngFrom, lngTo, text, options);
};

const translateReady = (id, data) => {
  const panel = document.querySelector(`#translation-output-${id}`);
  if (!panel) return; // in case the panel was already closed
  // panel.querySelector('.panel .input .html').innerHTML = data.source;
  // panel.querySelector('.panel .input .raw').innerText = data.source;
  panel.querySelector('.output .html').srcdoc = data.translated;
  panel.querySelector('.output .raw').innerText = addControlCharacters(data.translated);
  data.alignments.forEach(sentence => panel.querySelector('.alignments').appendChild(renderAlignmentsTable(sentence)));
}

// From html.cpp
function argmax(arr) {
  if (arr.length === 0) return undefined;
  return arr.reduce(((best, curr, i) => curr > arr[best] ? i : best), 0);
}

function isContinuation(token) {
  const delimiters = document.querySelector("#html-continuation-delimiters").value;  // TODO could have changed since translation request
  if (delimiters == "disable") return false;
  token = token.replace(/\<\/?.+?>/g, ''); // Remove HTML
  return token.length > 0 && delimiters.indexOf(token.substr(0, 1)) === -1;
}

function hardAlignments({originalTokens, translatedTokens, scores}) {
  return translatedTokens.map((_, t) => argmax(scores[t]));
}

function alignmentHeuristics(selected, {originalTokens, translatedTokens, scores}) {
  // EOS always aligns with EOS
  selected[translatedTokens.length - 1] = originalTokens.length - 1;

  for (let t = 1; t < translatedTokens.length - 1; ++t) { // -1 to exclude EOS
    if (isContinuation(translatedTokens[t])) {
      if (scores[t][selected[t]] > scores[t-1][selected[t-1]]) {
        for (let i = t; ; --i) {
          selected[i] = selected[t];
          if (i == 0 || !isContinuation(translatedTokens[i])) break;
        }
      }
    }
  }

  return selected;
}

function renderAlignmentsTable({originalTokens, translatedTokens, scores}) {
  const original = hardAlignments({originalTokens, translatedTokens, scores});

  const selected = alignmentHeuristics(original.slice(), {originalTokens, translatedTokens, scores});

  const classNames = (t, o) => {
    const list = [];
    if (original[t] === o)
      list.push('selected-argmax');
    if (selected[t] === o)
      list.push('selected-heuristic');
    return list.join(' ');
  };

  return html`<table>
    <tr>
      <th></th>
      ${originalTokens.map(token => html`<th>${addControlCharacters(token)}</th>`)}
    </tr>
    ${translatedTokens.map((token, t) => html`
      <tr>
        <th>${addControlCharacters(token)}</th>
        ${originalTokens.map((_, o) => html`<td className=${classNames(t,o)}>${scores[t][o].toFixed(4)}</td>`)}
      </tr>
    `)}
  </table>`;
}

worker.onmessage = function (e) {
  if (e.data[1] === "translate_reply" && e.data[2]) {
    translateReady(e.data[0], e.data[2]);
  } else if (e.data[1] === "load_model_reply" && e.data[2]) {
    status(e.data[2]);
    if (e.data[2].indexOf('successfully') !== undefined)
      document.querySelector("#translate-btn").disabled = false;
  } else if (e.data[1] === "import_reply" && e.data[2]) {
    modelRegistry = e.data[2];
    init();
  } else if (e.data[1] === "error" && e.data[2]) {
    status(`Error: ${e.data[2]}`);
  }
};

langs.forEach(([code, name]) => {
  langFrom.options.add(new Option(name, code));
  langTo.options.add(new Option(name, code));
});

const loadModel = () => {
  const lngFrom = langFrom.value;
  const lngTo = langTo.value;
  if (lngFrom !== lngTo) {
    status(`Installing model...`);
    console.log(`Loading model '${lngFrom}${lngTo}'`);
    document.querySelector("#translate-btn").disabled = true;
    worker.postMessage([0, "load_model", lngFrom, lngTo]);
  }
};

langFrom.addEventListener("change", e => {
  loadModel();
});

langTo.addEventListener("change", e => {
  window.localStorage['lang-to'] = langTo.value;
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

  if (langs.some(([code]) => code === window.localStorage['lang-to']))
    // If we remember an existing language from last time, load that
    langTo.value = window.localStorage['lang-to'];
  else
    // find first output lang that *isn't* input language
    langTo.value = langs.find(([code]) => code !== langFrom.value)[0];
  
  // load this model
  loadModel();
}

function addControlCharacters(text) {
  return text.replace(/\s/g, match => {
    switch (match[0]) {
      case " ":
        return "␣\u200b";
      case "\t":
        return "⎯\u200b";
      case "\n":
        return "↵\n";
      default:
        return match[0];
    }
  });
}

async function googleTranslate(id, lngFrom, lngTo, text, options) {
  if (!window.localStorage['GOOGLE_TRANSLATE_KEY'])
    return;

  const body = new FormData();
  body.append("q", text);
  body.append("source", langFrom.value);
  body.append("target", langTo.value);
  body.append("format", document.querySelector("#input-is-html").checked ? "html" : "text");
  body.append("key", window.localStorage['GOOGLE_TRANSLATE_KEY']);

  const response = await fetch("https://translation.googleapis.com/language/translate/v2", {method: "POST", body});
  const data = await response.json();
  const translated = data.data.translations[0].translatedText;

  const panel = document.querySelector(`#translation-output-${id}`);
  panel.querySelector('.gtranslate .html').srcdoc = translated;
  panel.querySelector('.gtranslate .raw').innerText = addControlCharacters(translated);
}