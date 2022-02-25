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

const textToHTML = (text) => {
  const div = document.createElement('div');
  div.appendChild(document.createTextNode(text));
  return div.innerHTML;
};

const translateCall = () => {
  const text = document.querySelector("#input").value;
  if (!text.trim().length) return;

  const paragraphs = text.split(/\n+/).map(textToHTML); // escape HTML 
  const translateOptions = _prepareTranslateOptions(paragraphs);
  const lngFrom = langFrom.value;
  const lngTo = langTo.value;
  worker.postMessage(["translate", lngFrom, lngTo, paragraphs, translateOptions]);
};

const addQualityClasses = (root) => {
  // You can do this wit CSS variables, calc() and min/max, but JS is just easier

  root.querySelectorAll('[x-bergamot-sentence-score]').forEach(el => {
    // Note: these thresholds are just examples, they are not good thresholds!  
    el.classList.toggle('bad', parseFloat(el.getAttribute('x-bergamot-sentence-score')) > -0.1);
  });

  root.querySelectorAll('[x-bergamot-word-score]').forEach(el => {
    // Note: these thresholds are just examples, they are not good thresholds!
    el.classList.toggle('bad', parseFloat(el.getAttribute('x-bergamot-word-score')) > -0.1);
  });

  // Add tooltips to each (sub)word with sentence and word score.
  root.querySelectorAll('[x-bergamot-sentence-score] > [x-bergamot-word-score]').forEach(el => {
    const sentenceScore = parseFloat(el.parentNode.getAttribute('x-bergamot-sentence-score'));
    const wordScore = parseFloat(el.getAttribute('x-bergamot-word-score'));
    el.title = `Sentence: ${sentenceScore}  Word: ${wordScore}`;
  });
}

worker.onmessage = function (e) {
  if (e.data[0] === "translate_reply" && e.data[1]) {
    // Clear output of previous translation
    document.querySelector("#output").innerHTML = '';

    // Add each translation in its own div to have a known root in which the
    // sentence ids are unique. Used for highlighting sentences.
    e.data[1].forEach(translatedHTML => {
      const translation = document.createElement('div');
      translation.classList.add('translation');
      translation.innerHTML = translatedHTML;
      addQualityClasses(translation);
      document.querySelector("#output").appendChild(translation);
    });
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
    const input = textToHTML(document.querySelector("#input").value);
    document.querySelector("#output").innerHTML = input;
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

$('#output').addEventListener('mouseover', e => {
  const root = e.target.closest('.translation');
  const sentence = e.target.parentNode.hasAttribute('x-bergamot-sentence-index') ? e.target.parentNode.getAttribute('x-bergamot-sentence-index') : null;  
  document.querySelectorAll('#output font[x-bergamot-sentence-index]').forEach(el => {
    el.classList.toggle('highlight-sentence', el.getAttribute('x-bergamot-sentence-index') === sentence && el.closest('.translation') === root);
  })
})

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
