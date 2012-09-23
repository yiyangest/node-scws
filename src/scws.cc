#include <v8.h>
#include <node.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <scws/scws.h>
#include <iostream>
#include <list>

using namespace node;
using namespace std;
using namespace v8;

#define REQ_FUN_ARG(I, VAR) \
    if (args.Length() <= (I) || !args[I]->IsFunction()) \
        return ThrowException(Exception::TypeError( \
            String::New("Argument " #I " must be a function"))); \
    Local<Function> VAR = Local<Function>::Cast(args[I])


struct scws_segment{

    ~scws_segment(){
        delete [] segment;
        segment = NULL;
    }
    char *segment;
    float idf;
    char attr[3];
};

template <typename T>
struct baton_t {

    baton_t(){
        attr = NULL;
    }

    ~baton_t(){
        if (scws != NULL) {
            scws_free(scws);
        }
        delete [] attr;
        attr = NULL;
    };
    uv_work_t request;

    scws_t scws;
    std::string source;
    int limit;
    char* attr;
    bool hasword;
    list<T> results;
    std::string error_message;
    Persistent<Function> callback;
};



void get_segments(const std::string& source, scws_t scws, std::string& error_message, list<scws_segment *>& results){
    if (scws == NULL) {
        error_message = "scws need initialize";
        return;
    }
    if (source.empty()) {
        error_message = "source is empty";
        return;
    }
    int length = source.length();

    scws_res_t res, cur;

    scws_send_text(scws, source.c_str(), length);
    while((res=cur=scws_get_result(scws))){
        while(cur!=NULL){
            string str = source.substr(cur->off, cur->len);
            scws_segment *segment = new scws_segment();
            segment->idf = cur->idf;
            strcpy(segment->attr, cur->attr);
            segment->segment = new char[cur->len+1];
            strcpy(segment->segment, str.c_str());

            results.push_back(segment);

            cur = cur->next;
        }
        scws_free_result(res);

    }

}

void get_topwords(const std::string& source, int limit, const char* attr, scws_t scws, std::string& error_message, list<scws_top_t>& results){
    if (scws == NULL) {
        error_message = "scws need initialize";
        return;
    }
    if (source.empty()) {
        error_message = "source is empty";
        return;
    }
    int length = source.length();

    scws_top_t cur;

    char* attrs = NULL;
    if (attr != NULL) {
        attrs = new char[strlen(attr)+1];
        strcpy(attrs, attr);
    }
    scws_send_text(scws, source.c_str(), length);
    cur = scws_get_tops(scws, limit, attrs);

    while(cur != NULL){
        results.push_back(cur);

        cur = cur->next;
    }

    scws_free_tops(cur);
    delete [] attrs;
}

void get_words(const std::string& source, const char* attr, scws_t scws, std::string& error_message, list<scws_top_t>& results){
    if (scws == NULL) {
        error_message = "scws need initialize";
        return;
    }
    if (source.empty()) {
        error_message = "source is empty";
        return;
    }
    int length = source.length();

    scws_top_t cur;

    char* attrs = NULL;
    if (attr != NULL) {
        attrs = new char[strlen(attr)+1];
        strcpy(attrs, attr);
    }
    scws_send_text(scws, source.c_str(), length);
    cur = scws_get_words(scws, attrs);

    while(cur != NULL){
        results.push_back(cur);

        cur = cur->next;
    }

    scws_free_tops(cur);
    delete [] attrs;
}

void has_word(const std::string& source, const char* attr, scws_t scws, std::string& error_message, int& result){
    if (scws == NULL) {
        error_message = "scws need initialize";
        return;
    }
    if (source.empty()) {
        error_message = "source is empty";
        return;
    }
    int length = source.length();

    char* attrs = NULL;
    if (attr != NULL) {
        attrs = new char[strlen(attr)+1];
        strcpy(attrs, attr);
    }
    scws_send_text(scws, source.c_str(), length);
    result = scws_has_word(scws, attrs);

    delete [] attrs;
}


class Scws: ObjectWrap
{
    private:
        scws_t c_scws_obj;
    public:
        static Persistent<FunctionTemplate> s_ct;

        static void Init(Handle<Object> target)
        {
            s_ct = Persistent<FunctionTemplate>::New(FunctionTemplate::New(New));
            s_ct->InstanceTemplate()->SetInternalFieldCount(1);
            s_ct->SetClassName(String::NewSymbol("Scws"));

            // bind methods
            NODE_SET_PROTOTYPE_METHOD(s_ct, "segment", segment);
            NODE_SET_PROTOTYPE_METHOD(s_ct, "topwords", topwords);
            NODE_SET_PROTOTYPE_METHOD(s_ct, "getwords", getwords);
            NODE_SET_PROTOTYPE_METHOD(s_ct, "hasword", hasword);


            // expose class as Scws
            target->Set(String::NewSymbol("Scws"), s_ct->GetFunction());
        }

        Scws()
        {
        }

        ~Scws()
        {
            scws_free(c_scws_obj);
        }

        static Handle<Value> New(const Arguments& args){
            Scws *scws = new Scws();
            scws->Wrap(args.This());
            if (!(scws->c_scws_obj = scws_new())) {
                ThrowException(Exception::TypeError(String::New("initial failure")));
                return Undefined();
            }

            Local<Object> defaultOpts = Object::New();
            defaultOpts->Set(String::New("dict"), String::New("/usr/local/Cellar/scws/etc/dict.utf8.xdb"));
            defaultOpts->Set(String::New("charset"), String::New("utf8"));
            defaultOpts->Set(String::New("rule"), String::New("/usr/local/Cellar/scws/etc/rules.utf8.ini"));
            defaultOpts->Set(String::New("ignore"), Number::New(1));
            defaultOpts->Set(String::New("multi"), Number::New(SCWS_MULTI_NONE));
            defaultOpts->Set(String::New("duality"), Number::New(0));
            defaultOpts->Set(String::New("debug"), Number::New(0));
            if (args.Length() == 1 && args[0]->IsObject()) {
                Local<Object> obj = args[0]->ToObject();
                Local<Array> propertyNames = obj -> GetOwnPropertyNames();
                int propertyLength = propertyNames->Length();
                for ( int i = 0; i < propertyLength; i ++) {
                    const Local<String>& propertyName = propertyNames->Get(i)->ToString();
                    const Local<Value>& propertyValue = obj->Get(propertyName);
                    defaultOpts->Set(propertyName, propertyValue);
                }
            }


            String::Utf8Value charset(defaultOpts->Get(String::New("charset")));
            scws_set_charset(scws->c_scws_obj, *charset);

            String::Utf8Value dict(defaultOpts->Get(String::New("dict")));
            scws_set_dict(scws->c_scws_obj, *dict, SCWS_XDICT_XDB);

            String::Utf8Value rule(defaultOpts->Get(String::New("rule")));
            scws_set_rule(scws->c_scws_obj, *rule);

            int multi = defaultOpts->Get(String::New("multi"))->ToInt32()->Value();
            scws_set_multi(scws->c_scws_obj, multi);

            int duality = defaultOpts->Get(String::New("duality"))->ToInt32()->Value();
            scws_set_duality(scws->c_scws_obj, duality);

            int debug = defaultOpts->Get(String::New("debug"))->ToInt32()->Value();
            scws_set_debug(scws->c_scws_obj, debug);

            // 忽略标点符号
            int ignore = defaultOpts->Get(String::New("ignore"))->ToInt32()->Value();
            scws_set_ignore(scws->c_scws_obj, ignore);
            return args.This();
        }


        // argument 0: source text
        // argument 1: callback
        static Handle<Value> segment(const Arguments& args){

            HandleScope scope;
            int length = args.Length();
            REQ_FUN_ARG(length - 1, callback);
            Handle<Value> arg0 = args[0];
            String::Utf8Value txt(arg0);


            Scws *scws = ObjectWrap::Unwrap<Scws>(args.This());

            baton_t<scws_segment *> *baton = new baton_t<scws_segment *>();
            baton->scws = scws_fork(scws->c_scws_obj);
            baton->source = *txt;
            baton->request.data = baton;
            baton->callback = Persistent<Function>::New(callback);
            baton->attr = NULL;

            uv_queue_work(uv_default_loop(), &baton->request, AsyncSegment,AfterSegment);
            //scws->Ref();

            //eio_custom(EIO_segment, EIO_PRI_DEFAULT, EIO_AfterSegment, baton);

            //ev_ref(EV_DEFAULT_UC);

            return Undefined();
        }

        static void AsyncSegment(uv_work_t *req){
            baton_t<scws_segment *> *baton = static_cast<baton_t<scws_segment *> *>(req->data);

            list<scws_segment *> results;
            std::string error_message;
            //printf("segment source :%s\n", baton->source);
            get_segments(baton->source, baton->scws, error_message,results);

            if (error_message.empty()) {
                baton->results = results;
            } else {
                baton->error_message = error_message;
            }

        }

        static void AfterSegment(uv_work_t *req){
            HandleScope scope;
            baton_t<scws_segment *> *baton = static_cast<baton_t<scws_segment *> *>(req->data);

            //ev_unref(EV_DEFAULT_UC);
            //baton->scws->Unref();

            if (!baton->error_message.empty()) {
                Local<Value> err = Exception::Error(
                        String::New(baton->error_message.c_str()));
                Local<Value> argv[] = {err};

                TryCatch try_catch;
                baton->callback->Call(Context::GetCurrent()->Global(), 1, argv);

                if (try_catch.HasCaught()) {
                    FatalException(try_catch);
                }
            } else {
                Local<Array> words = Array::New();
                int index = 0;
                //list<scws_segment *> *result = static_cast<list<scws_segment *> *>(baton->results);
                list<scws_segment *> results = baton->results;
                //list<scws_segment *> results = *result;
                list<scws_segment *>::iterator it;
                for (it=results.begin();it!=results.end();it++){
                    Local<Object> element = Object::New();
                    element->Set(String::New("word"), String::New((*it)->segment));
                    element->Set(String::New("weight"), Number::New((*it)->idf));
                    element->Set(String::New("attr"), String::New((*it)->attr));
                    words->Set(index, element);
                    //delete [] ((*it)->attr);
                    delete *it;
                    index += 1;
                }
                results.clear();
                Local<Value> argv[] = {
                    Local<Value>::New(Null()),
                    words
                };

                TryCatch try_catch;
                baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);

                if (try_catch.HasCaught()) {
                    FatalException(try_catch);
                }
            }

            baton->callback.Dispose();
            delete baton;
        }

        // argument 0: source text
        // argument 1: limit
        // argument 2: attr
        // argument 3: callback
        static Handle<Value> topwords(const Arguments& args){

            HandleScope scope;
            int length = args.Length();
            REQ_FUN_ARG(length - 1, callback);
            Handle<Value> arg0 = args[0];
            String::Utf8Value txt(arg0);

            int limit = 0;
            char *attr = NULL;

            if (length == 3) {
                if (args[1]->IsNumber()) {
                    limit = args[1]->NumberValue();
                }
            } else if (length == 4) {
                if (args[1]->IsNumber()) {
                    limit = args[1]->NumberValue();
                }
                if (args[2]->IsNull()) {
                    attr = NULL;
                } else {
                    Handle<Value> attrArg = args[2];
                    String::Utf8Value t_attr(attrArg);
                    attr = new char[t_attr.length() + 1];
                    strcpy(attr, *t_attr);
                }
            }


            Scws *scws = ObjectWrap::Unwrap<Scws>(args.This());

            baton_t<scws_top_t> *baton = new baton_t<scws_top_t>();
            baton->scws = scws_fork(scws->c_scws_obj);
            baton->source = *txt;
            baton->request.data = baton;
            baton->limit = limit;
            baton->attr = attr;
            baton->callback = Persistent<Function>::New(callback);

            uv_queue_work(uv_default_loop(), &baton->request, AsyncTopwords, AfterTopwords);

            return Undefined();
        }

        static void AsyncTopwords(uv_work_t *req){
            baton_t<scws_top_t> *baton = static_cast<baton_t<scws_top_t> *>(req->data);

            list<scws_top_t> results;
            std::string error_message;

            get_topwords(baton->source, baton->limit, baton->attr, baton->scws, error_message,results);

            if (error_message.empty()) {
                baton->results = results;
            } else {
                baton->error_message = error_message;
            }
        }

        static void AfterTopwords(uv_work_t *req){
            HandleScope scope;
            baton_t<scws_top_t> *baton = static_cast<baton_t<scws_top_t> *>(req->data);


            if (!baton->error_message.empty()) {
                Local<Value> err = Exception::Error(
                        String::New(baton->error_message.c_str()));
                Local<Value> argv[] = {err};

                TryCatch try_catch;
                baton->callback->Call(Context::GetCurrent()->Global(), 1, argv);

                if (try_catch.HasCaught()) {
                    FatalException(try_catch);
                }
            } else {
                Local<Array> words = Array::New();
                int index = 0;
                //list<scws_segment *> *result = static_cast<list<scws_segment *> *>(baton->results);
                list<scws_top_t> results = baton->results;
                //list<scws_segment *> results = *result;
                list<scws_top_t>::iterator it;
                for (it=results.begin();it!=results.end();it++){
                    Local<Object> element = Object::New();
                    element->Set(String::New("word"), String::New((*it)->word));
                    element->Set(String::New("weight"), Number::New((*it)->weight));
                    element->Set(String::New("times"), Number::New((*it)->times));

                    // char attr[2] sometimes isnt ended with '\0'
                    // so we need new String with explicity length
                    int attrLength = strlen((*it)->attr);
                    attrLength = attrLength > 2 ? 2: attrLength;
                    element->Set(String::New("attr"), String::New((*it)->attr,attrLength));

                    words->Set(index, element);
                    //delete [] (*it)->segment;
                    index += 1;
                }
                Local<Value> argv[] = {
                    Local<Value>::New(Null()),
                    words
                };

                TryCatch try_catch;
                baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);

                if (try_catch.HasCaught()) {
                    FatalException(try_catch);
                }
            }

            baton->callback.Dispose();
            delete baton;
        }


        // argument 0: source text
        // argument 1: attr
        // argument 2: callback
        static Handle<Value> getwords(const Arguments& args){

            HandleScope scope;
            int length = args.Length();
            REQ_FUN_ARG(length - 1, callback);
            Handle<Value> arg0 = args[0];
            String::Utf8Value txt(arg0);

            char *attr = NULL;

            if (length == 3) {
                if (args[1]->IsNull()) {
                    attr = NULL;
                } else {
                    Handle<Value> attrArg = args[1];
                    String::Utf8Value t_attr(attrArg);
                    attr = new char[t_attr.length() + 1];
                    strcpy(attr, *t_attr);
                }
            }


            Scws *scws = ObjectWrap::Unwrap<Scws>(args.This());

            baton_t<scws_top_t> *baton = new baton_t<scws_top_t>();
            baton->scws = scws_fork(scws->c_scws_obj);
            baton->source = *txt;
            baton->request.data = baton;
            baton->attr = attr;
            baton->callback = Persistent<Function>::New(callback);

            uv_queue_work(uv_default_loop(), &baton->request, AsyncGetwords, AfterGetwords);

            return Undefined();
        }

        static void AsyncGetwords(uv_work_t *req){
            baton_t<scws_top_t> *baton = static_cast<baton_t<scws_top_t> *>(req->data);

            list<scws_top_t> results;
            std::string error_message;

            get_words(baton->source, baton->attr, baton->scws, error_message,results);

            if (error_message.empty()) {
                baton->results = results;
            } else {
                baton->error_message = error_message;
            }
        }

        static void AfterGetwords(uv_work_t *req){
            HandleScope scope;
            baton_t<scws_top_t> *baton = static_cast<baton_t<scws_top_t> *>(req->data);


            if (!baton->error_message.empty()) {
                Local<Value> err = Exception::Error(
                        String::New(baton->error_message.c_str()));
                Local<Value> argv[] = {err};

                TryCatch try_catch;
                baton->callback->Call(Context::GetCurrent()->Global(), 1, argv);

                if (try_catch.HasCaught()) {
                    FatalException(try_catch);
                }
            } else {
                Local<Array> words = Array::New();
                int index = 0;
                //list<scws_segment *> *result = static_cast<list<scws_segment *> *>(baton->results);
                list<scws_top_t> results = baton->results;
                //list<scws_segment *> results = *result;
                list<scws_top_t>::iterator it;
                for (it=results.begin();it!=results.end();it++){
                    Local<Object> element = Object::New();
                    element->Set(String::New("word"), String::New((*it)->word));
                    element->Set(String::New("weight"), Number::New((*it)->weight));
                    element->Set(String::New("times"), Number::New((*it)->times));

                    // char attr[2] sometimes isnt ended with '\0'
                    // so we need new String with explicity length
                    int attrLength = strlen((*it)->attr);
                    attrLength = attrLength > 2 ? 2: attrLength;
                    element->Set(String::New("attr"), String::New((*it)->attr,attrLength));

                    words->Set(index, element);
                    //delete [] (*it)->segment;
                    index += 1;
                }
                Local<Value> argv[] = {
                    Local<Value>::New(Null()),
                    words
                };

                TryCatch try_catch;
                baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);

                if (try_catch.HasCaught()) {
                    FatalException(try_catch);
                }
            }

            baton->callback.Dispose();
            delete baton;
        }


        // argument 0: source text
        // argument 1: attr
        // argument 2: callback
        static Handle<Value> hasword(const Arguments& args){

            HandleScope scope;
            int length = args.Length();
            REQ_FUN_ARG(length - 1, callback);
            Handle<Value> arg0 = args[0];
            String::Utf8Value txt(arg0);

            char *attr = NULL;

            if (length == 3) {
                if (args[1]->IsNull()) {
                    attr = NULL;
                } else {
                    Handle<Value> attrArg = args[1];
                    String::Utf8Value t_attr(attrArg);
                    attr = new char[t_attr.length() + 1];
                    strcpy(attr, *t_attr);
                }
            }


            Scws *scws = ObjectWrap::Unwrap<Scws>(args.This());

            baton_t<scws_top_t> *baton = new baton_t<scws_top_t>();
            baton->scws = scws_fork(scws->c_scws_obj);
            baton->source = *txt;
            baton->request.data = baton;
            baton->attr = attr;
            baton->callback = Persistent<Function>::New(callback);

            uv_queue_work(uv_default_loop(), &baton->request, AsyncHasword, AfterHasword);

            return Undefined();
        }

        static void AsyncHasword(uv_work_t *req){
            baton_t<scws_top_t> *baton = static_cast<baton_t<scws_top_t> *>(req->data);

            int has = 0;
            std::string error_message;

            has_word(baton->source, baton->attr, baton->scws, error_message, has);

            if (error_message.empty()) {
                baton->hasword = has;
            } else {
                baton->error_message = error_message;
            }
        }

        static void AfterHasword(uv_work_t *req){
            HandleScope scope;
            baton_t<scws_top_t> *baton = static_cast<baton_t<scws_top_t> *>(req->data);


            if (!baton->error_message.empty()) {
                Local<Value> err = Exception::Error(
                        String::New(baton->error_message.c_str()));
                Local<Value> argv[] = {err};

                TryCatch try_catch;
                baton->callback->Call(Context::GetCurrent()->Global(), 1, argv);

                if (try_catch.HasCaught()) {
                    FatalException(try_catch);
                }
            } else {

                Local<Boolean> hasword = Local<Boolean>::New(Boolean::New(baton->hasword));

                Local<Value> argv[] = {
                    Local<Value>::New(Null()),
                    hasword
                };

                TryCatch try_catch;
                baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);

                if (try_catch.HasCaught()) {
                    FatalException(try_catch);
                }
            }

            baton->callback.Dispose();
            delete baton;
        }
};

Persistent<FunctionTemplate> Scws::s_ct;
extern "C" {
    static void init(Handle<Object> target)
    {
        Scws::Init(target);
    }
    NODE_MODULE(node_scws,init);
}
