/*
 * @author - Based of a file from Gist here: https://gist.github.com/1757658
 *
 * @modified - Mike Newell - it was on Gist so I figure I can use it
 *
 * @Description -   Added support for a few more mime types including the new
 *                  .ogv, .webm, and .mp4 file types for HTML5 video.
 *
 */

/*
* @modified - Andre Natal - removed unused types for the purpose of this use
case
*/

Helper = {

    types: {
       "wasm" : "application/wasm"
       , "js" : "application/javascript"
       , "html" : "text/html"
       , "htm" : "text/html"
       , "ico" : "image/vnd.microsoft.icon",
    },

    getMime: function(u) {

        var ext = this.getExt(u.pathname).replace('.', '');

        return this.types[ext.toLowerCase()] || 'application/octet-stream';

    },

    getExt: function(path) {
        var i = path.lastIndexOf('.');

        return (i < 0) ? '' : path.substr(i);
    }

};
