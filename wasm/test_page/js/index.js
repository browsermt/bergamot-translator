function $(selector) {
  return document.querySelector(selector);
}

function encodeHTML(text) {
  const div = document.createElement('div');
  div.appendChild(document.createTextNode(text));
  return div.innerHTML;
}

const translator = new BergamotLatencyOptimisedTranslator({
  cacheSize: 2^13,
  workerUrl: 'js/translator-worker.js'
});

async function translate() {
  try {
    const response = await translator.translate({
      from: $('#lang-from').value,
      to: $('#lang-to').value,
      text: $('#input').innerHTML,
      html: true
    });

    $('#output').innerHTML = response.target.text;
  } catch (error) {
    if (error.constructor === SupersededError)
      return;

    alert(error.toString());
  }
}

translator.registry.then(models => {
  const names = new Intl.DisplayNames(['en'], {type: 'language'});

  ['from', 'to'].forEach(field => {
    const languages = new Set(models.map(model => model[field]));
    const select = $(`#lang-${field}`);

    const pairs = Array.from(languages, code => ({code, name: names.of(code)}));
    
    pairs.sort(({name: a}, {name: b}) => a.localeCompare(b));

    pairs.forEach(({name, code}) => {
      select.add(new Option(name, code));
    })
  });

  $('#lang-from').value = 'de';
  $('#lang-to').value = 'en';
});

$('button.swap').addEventListener('click', () => {
  const tmp = $('#lang-from').value;
  $('#lang-from').value = $('#lang-to').value;
  $('#lang-to').value = tmp;
  translate();
})

$('#input').addEventListener('input', translate);
$('#lang-from').addEventListener('input', translate);
$('#lang-to').addEventListener('input', translate);