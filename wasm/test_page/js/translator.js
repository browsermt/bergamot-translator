/**
 * @typedef {Object} TranslationRequest
 * @property {String} from
 * @property {String} to
 * @property {String} text
 * @property {Boolean} html
 * @property {Integer?} priority
 */

// Little work-around for Safari
if (typeof requestIdleCallback === 'undefined') {
    function requestIdleCallback(callback, options) {
        setTimeout(callback, 0);
    }
}

/**
 * Wrapper around bergamot-translator and model management. You only need
 * to call translate() which is async, the helper will manage execution by
 * itself.
 */
 class BergamotTranslator {
    
    /**
     * @param {{
     *  cacheSize?: number,
     *  useNativeIntGemm?: boolean,
     *  downloadTimeout?: number,
     *  workerUrl?: string,
     *  registryUrl?: string
     *  pivotLanguage?: string?
     * }} options
     */
    constructor(options) {
        this.options = options || {};

        this.registryUrl = options.registryUrl || 'https://storage.googleapis.com/bergamot-models-sandbox/0.3.3/registry.json';

        this.downloadTimeout = options.downloadTimeout || 60000;

        /**
         * registry of all available models and their urls
         * @type {Promise<Model[]>}
         */
        this.registry = this.loadModelRegistery();

        /**
         * Map of downloaded model data files as buffers per model.
         * @type {Map<{from:string,to:string}, Promise<Map<string,ArrayBuffer>>>}
         */
        this.buffers = new Map();

        /**
         * @type {string?}
         */
        this.pivotLanguage = 'pivotLanguage' in options ? options.pivotLanguage : 'en';
        
        /**
         * A map of language-pairs to a list of models you need for it.
         * @type {Map<{from:string,to:string}, Promise<{from:string,to:string}[]>>}
         */
        this.models = new Map();

        /**
         * @type {string} URL for Web worker
         */
        this.workerUrl = options.workerUrl || 'translation-worker.js';

        /**
         * Error handler for all errors that are async, not tied to a specific
         * call and that are unrecoverable.
         * @type {(error: Error)}
         */
        this.onerror = err => console.error('WASM Translation Worker error:', err);
    }

    /**
     * Destructor that stops and cleans up.
     */
    delete() {
        // Empty the queue
        this.remove(() => true);

        // Terminate the workers
        this.workers.forEach(({worker}) => worker.terminate());

        // Remove any references to data we hold
        this.buffers.clear();
    }

    /**
     * Loads a worker thread, and wraps it in a message passing proxy. I.e. it
     * exposes the entire interface of TranslationWorker here, and all calls
     * to it are async. Do note that you can only pass arguments that survive
     * being copied into a message. 
     * @return {{worker:Worker, client:Proxy<TranslationWorker>}}
     */
    loadWorker() {
        const worker = new Worker(this.workerUrl);
        worker.onerror = this.onerror.bind(this);

        // Initialisation options
        worker.postMessage({options: this.options});

        /**
         * Map of pending requests
         * @type {Map<number,{accept:(any), reject:(Error)}>}
         */
        const pending = new Map();

        /**
         * Incremental counter to derive request/response ids from.
         */
        let serial = 0;

        // Receive responses
        worker.onmessage = function({data: {id, result, error}}) {
            if (!pending.has(id)) {
                console.debug('Received message with unknown id:', arguments[0]);
                throw new Error(`BergamotTranslator received response from worker to unknown call '${id}'`);
            }

            const {accept, reject} = pending.get(id);
            pending.delete(id);

            if (error !== undefined)
                reject(Object.assign(new Error(), error));
            else
                accept(result);
        }

        /**
         * Little wrapper around the message passing api of Worker to make it
         * easy to await a response to a sent message. This wraps the worker in
         * a Proxy so you can treat it as if it is an instance of the
         * TranslationWorker class that lives inside the worker. All function
         * calls to it are transparently passed through the message passing
         * channel.
         */
        return {
            worker,
            client: new Proxy({}, {
                get(target, name, receiver) {
                    // Prevent this object from being marked "then-able"
                    if (name === 'then')
                        return undefined;

                    return (...args) => new Promise((accept, reject) => {
                        const id = ++serial;
                        pending.set(id, {accept, reject});
                        worker.postMessage({id, name, args});
                    });
                }
            })
        };
    }

    /**
     * Loads the model registry. Uses the registry shipped with this extension,
     * but formatted a bit easier to use, and future-proofed to be swapped out
     * with a TranslateLocally type registry.
     * @return {Promise<{
     *   from: string,
     *   to: string,
     *   files: {
     *     [part:string]: {
     *       name: string,
     *       size: number,
     *       expectedSha256Hash: string
     *     }
     *   }[]
     * }>}
     */
    async loadModelRegistery() {
        const response = await fetch(this.registryUrl);
        const registry = await response.json();

        // Add 'from' and 'to' keys for each model.
        return Array.from(Object.entries(registry), ([key, files]) => {
            return {
                from: key.substring(0, 2),
                to: key.substring(2, 4),
                files
            }
        });
    }

    /**
     * Gets or loads translation model data. Caching wrapper around
     * `loadTranslationModel()`.
     * @param {{from:string, to:string}}
     * @return {Promise<{
     *   model: ArrayBuffer,
     *   vocab: ArrayBuffer,
     *   shortlist: ArrayBuffer,
     *   qualityModel: ArrayBuffer?
     * }>}
     */
    getTranslationModel({from, to}) {
        const key = JSON.stringify({from, to});

        if (!this.buffers.has(key))
            this.buffers.set(key, this.loadTranslationModel({from, to}));

        return this.buffers.get(key);
    }

    /**
     * Downloads a translation model and returns a set of
     * ArrayBuffers. These can then be passed to a TranslationWorker thread
     * to instantiate a TranslationModel inside the WASM vm.
     * @param {{from:string, to:string}}
     * @return {Promise<{
     *   model: ArrayBuffer,
     *   vocab: ArrayBuffer,
     *   shortlist: ArrayBuffer,
     *   qualityModel: ArrayBuffer?
     *   config: string?
     * }>}
     */
    async loadTranslationModel({from, to}) {
        performance.mark(`loadTranslationModule.${JSON.stringify({from, to})}`);

        // Subdirectory where all model files reside
        const baseUrl = this.registryUrl.substring(0, this.registryUrl.lastIndexOf('/'));

        // Find that model in the registry which will tell us about its files
        const entries = (await this.registry).filter(model => model.from == from && model.to == to);

        if (!entries)
            throw new Error(`No model for '${from}' -> '${to}'`);

        const files = entries[0].files;

        // Download all files mentioned in the registry entry.
        const buffers = Object.fromEntries(await Promise.all(Array.from(Object.entries(files), async ([part, file]) => {
            // Special case where qualityModel is not part of the model
            if (file === undefined)
                return [part, null];

            try {
                return [part, await this.fetch(`${baseUrl}/${from}${to}/${file.name}`, file.expectedSha256Hash)];
            } catch (cause) {
                throw new Error(`Could not fetch ${file.name} for ${from}->${to} model`, {cause});
            }
        })));

        performance.measure('loadTranslationModel', `loadTranslationModule.${JSON.stringify({from, to})}`);

        let vocabs = [];

        if (buffers.vocab)
            vocabs = [buffers.vocab]
        else if (buffers.trgvocab && buffers.srcvocab)
            vocabs = [buffers.srcvocab, buffers.trgvocab]
        else
            throw new Error(`Could not identify vocab files for ${from}->${to} model among: ${Array.from(Object.keys(files)).join(' ')}`);

        let config = {};

        // For the Ukrainian models we need to override the gemm-precision
        if (files.model.name.endsWith('intgemm8.bin'))
            config['gemm-precision'] = 'int8shiftAll';

        // If quality estimation is used, we need to turn off skip-cost. Turning
        // this off causes quite the slowdown.
        if (files.qualityModel)
            config['skip-cost'] = false;

        // Translate to generic bergamot-translator format that also supports
        // separate vocabularies for input & output language, and calls 'lex'
        // a more descriptive 'shortlist'.
        return {
            model: buffers.model,
            shortlist: buffers.lex,
            vocabs,
            qualityModel: buffers.qualityModel,
            config
        };
    }

    /**
     * Helper to download file from the web (and store it in the cache if that
     * is passed in as well). Verifies the checksum.
     * @param {string} url
     * @param {string} checksum sha256 checksum as hexadecimal string
     * @param {Cache?} cache optional cache to save response into
     * @returns {Promise<ArrayBuffer>}
     */
    async fetch(url, checksum) {
        // Rig up a timeout cancel signal for our fetch
        const abort = new AbortController();
        const timeout = setTimeout(() => abort.abort(), this.downloadTimeout);

        // Start downloading the url, using the hex checksum to ask
        // `fetch()` to verify the download using subresource integrity 
        const response = await fetch(url, {
            integrity: `sha256-${this.hexToBase64(checksum)}`,
            signal: abort.signal
        });

        // Finish downloading (or crash due to timeout)
        const buffer = await response.arrayBuffer();

        // Download finished, remove the abort timer
        clearTimeout(timeout);

        return buffer;
    }

    /**
     * Conv erts the hexadecimal hashes from the registry to something we can use with
     * the fetch() method.
     */
    hexToBase64(hexstring) {
        return btoa(hexstring.match(/\w{2}/g).map(function(a) {
            return String.fromCharCode(parseInt(a, 16));
        }).join(""));
    }

    /**
     * Crappy named method that gives you a list of models to translate from
     * one language into the other. Generally this will be the same as you
     * just put in if there is a direct model, but it could return a list of
     * two models if you need to pivot through a third language.
     * Returns just [{from:str,to:str}...]. To be used something like this:
     * ```
     * const models = await this.getModels(from, to);
     * models.forEach(({from, to}) => {
     *   const buffers = await this.loadTranslationModel({from,to});
     *   [TranslationWorker].loadTranslationModel({from,to}, buffers)
     * });
     * ```
     * @returns {Promise<TranslationModel[]>}
     */
    getModels({from, to}) {
        const key = JSON.stringify({from, to});

        // Note that the `this.models` map stores Promises. This so that
        // multiple calls to `getModels` that ask for the same model will
        // return the same promise, and the actual lookup is only done once.
        // The lookup is async because we need to await `this.registry`
        if (!this.models.has(key))
            this.models.set(key, this.findModels(from, to));

        return this.models.get(key);
    }

    /**
     * Find model (or model pair) to translate from `from` to `to`.
     * @param {string} from
     * @param {string} to
     * @returns {Promise<TranslationModel[]>}
     */
    async findModels(from, to) {
        const registry = await this.registry;

        let direct = [], outbound = [], inbound = [];

        registry.forEach(model => {
            if (model.from === from && model.to === to)
                direct.push(model);
            else if (model.from === from && model.to === this.pivotLanguage)
                outbound.push(model);
            else if (model.to === to && model.from === this.pivotLanguage)
                inbound.push(model);
        });

        if (direct.length)
            return [direct[0]];

        if (outbound.length && inbound.length)
            return [outbound[0], inbound[0]];

        throw new Error(`No model available to translate from '${from}' to '${to}'`);
    }
}

/**
 * Translator balancing between throughput and latency. Can use multiple worker
 * threads.
 */
class BergamotBatchTranslator extends BergamotTranslator {
    /**
     * @param {{
     *  cacheSize?: number,
     *  useNativeIntGemm?: boolean,
     *  workers?: number,
     *  batchSize?: number,
     *  downloadTimeout?: number,
     *  workerUrl?: string,
     *  registryUrl?: string
     *  pivotLanguage?: string?
     * }} options
     */
    constructor(options) {
        super(options);

        /**
         * @type {Array<{idle:Boolean, worker:Proxy}>} List of active workers
         * (and a flag to mark them idle or not)
         */
        this.workers = [];

        /**
         * Maximum number of workers
         * @type {number} 
         */
        this.workerLimit = Math.max(this.options.workers || 0, 1);

        /**
         * List of batches we push() to & shift() from using `enqueue`.
         * @type {{
         *    id: number,
         *    key: string,
         *    priority: number,
         *    models: TranslationModel[],
         *    requests: Array<{
         *      request: TranslationRequest,
         *      resolve: (response: TranslationResponse),
         *      reject: (error: Error)
         *    }>
         * }}
         */
        this.queue = [];

        /**
         * batch serial to help keep track of batches when debugging
         * @type {Number}
         */
        this.batchSerial = 0;

        /**
         * Number of requests in a batch before it is ready to be translated in
         * a single call. Bigger is better for throughput (better matrix packing)
         * but worse for latency since you'll have to wait for the entire batch
         * to be translated.
         * @type {Number}
         */
        this.batchSize = Math.max(this.options.batchSize || 8, 1);

        this.onerror = err => console.error('WASM Translation Worker error:', err);
    }
    /**
     * Makes sure queued work gets send to a worker. Will delay it till `idle`
     * to make sure the batches have been filled to some degree. Will keep
     * calling itself as long as there is work in the queue, but it does not
     * hurt to call it multiple times. This function always returns immediately.
     */
    notify() {
        requestIdleCallback(async () => {
            // Is there work to be done?
            if (!this.queue.length)
                return;

            // Find an idle worker
            let worker = this.workers.find(worker => worker.idle);

            // No worker free, but space for more?
            if (!worker && this.workers.length < this.workerLimit) {
                try {
                    worker = {
                        ...this.loadWorker(), // adds `worker` and `client` props
                        idle: true
                    };
                    this.workers.push(worker);
                } catch (e) {
                    this.onerror(new Error(`Could not initialise translation worker: ${e.message}`));
                }
            }

            // If no worker, that's the end of it.
            if (!worker)
                return;

            // Up to this point, this function has not used await, so no
            // chance that another call stole our batch since we did the check
            // at the beginning of this function and JavaScript is only
            // cooperatively parallel.
            const batch = this.queue.shift();

            // Put this worker to work, marking as busy
            worker.idle = false;
            try {
                await this.consumeBatch(batch, worker.client);
            } catch (e) {
                batch.requests.forEach(({reject}) => reject(e));
            }
            worker.idle = true;

            // Is there more work to be done? Do another idleRequest
            if (this.queue.length)
                this.notify();
        }, {timeout: 500});
    }

    /**
     * The only real public call you need!
     * ```
     * const {target: {text:string}} = await this.translate({
     *   from: 'de',
     *   to: 'en',
     *   text: 'Hallo Welt!',
     *   html: false, // optional
     *   priority: 0 // optional, like `nice` lower numbers are translated first
     * })
     * ```
     * @param {TranslationRequest} request
     * @returns {Promise<TranslationResponse>}
     */
    translate(request) {
        const {from, to, priority} = request;

        return new Promise(async (resolve, reject) => {
            try {
                // Batching key: only requests with the same key can be batched
                // together. Think same translation model, same options.
                const key = JSON.stringify({from, to});

                // (Fetching models first because if we would do it between looking
                // for a batch and making a new one, we end up with a race condition.)
                const models = await this.getModels(request);
                
                // Put the request and its callbacks into a fitting batch
                this.enqueue({key, models, request, resolve, reject, priority});

                // Tell a worker to pick up the work at some point.
                this.notify();
            } catch (e) {
                reject(e);
            }
        });
    }

    /**
     * Prune pending requests by testing each one of them to whether they're
     * still relevant. Used to prune translation requests from tabs that got
     * closed.
     * @param {(request:TranslationRequest) => boolean} filter evaluates to true if request should be removed
     */
    remove(filter) {
        const queue = this.queue;

        this.queue = [];

        queue.forEach(batch => {
            batch.requests.forEach(({request, resolve, reject}) => {
                if (filter(request)) {
                    // Add error.request property to match response.request for
                    // a resolve() callback. Pretty useful if you don't want to
                    // do all kinds of Funcion.bind() dances.
                    reject(Object.assign(new Error('removed by filter'), {request}));
                    return;
                }

                this.enqueue({
                    key: batch.key,
                    priority: batch.priority,
                    models: batch.models,
                    request,
                    resolve,
                    reject
                });
            });
        });
    }

    /**
     * Internal function used to put a request in a batch that still has space.
     * Also responsible for keeping the batches in order of priority. Called by
     * `translate()` but also used when filtering pending requests.
     * @param {{request:TranslateRequest, models:TranslationModel[], key:String, priority:Number?, resolve:(TranslateResponse)=>any, reject:(Error)=>any}}
     */
    enqueue({key, models, request, resolve, reject, priority}) {
        if (priority === undefined)
            priority = 0;
         // Find a batch in the queue that we can add to
         // (TODO: can we search backwards? that would speed things up)
        let batch = this.queue.find(batch => {
            return batch.key === key
                && batch.priority === priority
                && batch.requests.length < this.batchSize
        });

        // No batch or full batch? Queue up a new one
        if (!batch) {
            batch = {id: ++this.batchSerial, key, priority, models, requests: []};
            this.queue.push(batch);
            this.queue.sort((a, b) => a.priority - b.priority);
        }

        batch.requests.push({request, resolve, reject});
    }

    /**
     * Internal method that uses a worker thread to process a batch. You can
     * wait for the batch to be done by awaiting this call. You should only
     * then reuse the worker otherwise you'll just clog up its message queue.
     */
    async consumeBatch(batch, worker) {
        performance.mark('BTConsumeBatch.start');

        // Make sure the worker has all necessary models loaded. If not, tell it
        // first to load them.
        await Promise.all(batch.models.map(async ({from, to}) => {
            if (!await worker.hasTranslationModel({from, to})) {
                const buffers = await this.getTranslationModel({from, to});
                await worker.loadTranslationModel({from, to}, buffers);
            }
        }));

        // Call the worker to translate. Only sending the actually necessary
        // parts of the batch to avoid trying to send things that don't survive
        // the message passing API between this thread and the worker thread.
        const responses = await worker.translate({
            models: batch.models.map(({from, to}) => ({from, to})),
            texts: batch.requests.map(({request: {text, html, qualityScores}}) => ({
                text: text.toString(),
                html: !!html,
                qualityScores: !!qualityScores
            }))
        });

        // Responses are in! Connect them back to their requests and call their
        // callbacks.
        batch.requests.forEach(({request, resolve, reject}, i) => {
            // TODO: look at response.ok and reject() if it is false
            resolve({
                request, // Include request for easy reference? Will allow you
                         // to specify custom properties and use that to link
                         // request & response back to each other.
                ...responses[i] // {target: {text: String}}
            });
        });
        
        performance.measure('BTConsumeBatch', 'BTConsumeBatch.start');
    }
}

/**
 * Thrown when a pending translation is replaced by another newer pending
 * translation.
 */
class SupersededError extends Error {}

/**
 * Translator optimised for interactive use.
 */
class BergamotLatencyOptimisedTranslator extends BergamotTranslator {
    /**
     * @type {{idle:boolean, worker:Worker, client:Proxy<TranslationWorker>} | null}
     */
    worker = null;

    /**
     * @type {TranslationRequest | null}
     */
    pending = null;

    /**
     * @param {{
     *  cacheSize?: number,
     *  useNativeIntGemm?: boolean,
     *  downloadTimeout?: number,
     *  workerUrl?: string,
     *  registryUrl?: string
     *  pivotLanguage?: string?
     * }} options
     */
    constructor(options) {
        super(options);
    }

    translate(request) {
        if (this.pending)
            this.pending.reject(new SupersededError());
        
        return new Promise((accept, reject) => {
            this.pending = {request, accept, reject};
            this.notify();
        });
    }
    
    notify() {
        requestIdleCallback(async () => {
            if (!this.pending)
                return;

            if (!this.worker)
                this.worker = {
                    ...this.loadWorker(),
                    idle: true
                };

            if (!this.worker.idle)
                return;

            const task = this.pending;
            this.pending = null;

            this.worker.idle = false;
            const {client} = this.worker;

            try {
                const {request} = task;
                
                const models = await this.getModels(request)

                await Promise.all(models.map(async ({from, to}) => {
                    if (!await client.hasTranslationModel({from, to})) {
                        const buffers = await this.getTranslationModel({from, to});
                        await client.loadTranslationModel({from, to}, buffers);
                    }
                }));

                const {text, html, qualityScores} = request;
                const responses = await client.translate({
                    models: models.map(({from,to}) => ({from, to})),
                    texts: [{text, html, qualityScores}]
                });

                task.accept({request, ...responses[0]});
            } catch (e) {
                task.reject(e);
            }
            this.worker.idle = true;

            // Is there more work to be done? Do another idleRequest
            if (this.pending)
                this.notify();
        }, {timeout: 500});
    }
}
