// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_key_requirements.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's behavior adding inline and passed key simultaneously");

function test()
{
    removeVendorPrefixes();

    name = self.location.pathname;
    description = "My Test Database";
    request = evalAndLog("indexedDB.open(name, description)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    debug("openSuccess():");
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("request = db.setVersion('version 1')");
    request.onsuccess = cleanDatabase;
    request.onerror = unexpectedErrorCallback;
}

function cleanDatabase()
{
    debug("cleanDatabase():");
    deleteAllObjectStores(db);
    objectStore = evalAndLog("objectStore = db.createObjectStore('baz', { keyPath: 'id' });");
    evalAndExpectException("objectStore.add({id: 5}, 5);", "IDBDatabaseException.DATA_ERR");
    finishJSTest();
}

test();