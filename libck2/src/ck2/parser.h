// -*- c++ -*-

#pragma once
#include "common.h"

#include "error_queue.h"
#include "cstr_pool.h"
#include "cstr.h"
#include "lexer.h"
#include "date.h"
#include "fp_decimal.h"
#include "token.h"
#include "error.h"

#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <boost/filesystem.hpp>


_CK2_NAMESPACE_BEGIN;


using std::unique_ptr;
typedef fp_decimal<3> fp3;
class block;
class list;
class parser;


enum class binary_op {
    EQ,  // =
    LT,  // <
    GT,  // >
    LTE, // <=
    GTE, // >=
    EQ2, // ==
};


/* OBJECT -- generic "any"-type syntax tree node */

class object {
    enum {
        NIL,
        STRING,
        INTEGER,
        DATE,
        DECIMAL,
        BINARY_OP,
        BLOCK,
        LIST,
    } _type;

    using binop = binary_op;

    union data_union {
        char* s;
        int   i;
        date  d;
        fp3   f;
        binop o;
        unique_ptr<block> up_block;
        unique_ptr<list>  up_list;

        /* tell C++ that we'll manage the nontrivial union members outside of this union */
        data_union() {}
        ~data_union() {}
    } _data;

    floc _loc;

    void destroy() noexcept; // helper for dtor & move-assignment

public:
    object(const floc& fl = floc()) : _type(NIL),       _loc(fl) {}
    object(char* s, const floc& fl) : _type(STRING),    _loc(fl) { _data.s = s; }
    object(int i,   const floc& fl) : _type(INTEGER),   _loc(fl) { _data.i = i; }
    object(date d,  const floc& fl) : _type(DATE),      _loc(fl) { _data.d = d; }
    object(fp3 f,   const floc& fl) : _type(DECIMAL),   _loc(fl) { _data.f = f; }
    object(binop o, const floc& fl) : _type(BINARY_OP), _loc(fl) { _data.o = o; }
    object(unique_ptr<block> up, const floc& fl) : _type(BLOCK), _loc(fl) { new (&_data.up_block) unique_ptr<block>(std::move(up)); }
    object(unique_ptr<list> up,  const floc& fl) : _type(LIST),  _loc(fl) { new (&_data.up_list) unique_ptr<list>(std::move(up)); }

    /* move-assignment operator */
    object& operator=(object&& other);

    /* move-constructor (implemented via move-assignment) */
    object(object&& other) : object() { *this = std::move(other); }

    /* destructor */
    ~object() { destroy(); }

    floc const& location() const noexcept { return _loc; }
    floc&       location()       noexcept { return _loc; }
    // const comment_block* precomments() const noexcept { return _up_precomments.get(); }
    // const char*          postcomment() const noexcept { return _postcomment; }

    /* type accessors */
    bool is_null()      const noexcept { return _type == NIL; }
    bool is_string()    const noexcept { return _type == STRING; }
    bool is_integer()   const noexcept { return _type == INTEGER; }
    bool is_date()      const noexcept { return _type == DATE; }
    bool is_decimal()   const noexcept { return _type == DECIMAL; }
    bool is_binary_op() const noexcept { return _type == BINARY_OP; }
    bool is_block()     const noexcept { return _type == BLOCK; }
    bool is_list()      const noexcept { return _type == LIST; }
    bool is_number()    const noexcept { return is_integer() || is_decimal(); }

    /* data accessors (unchecked type) */
    char*  as_string()    const noexcept { return _data.s; }
    int    as_integer()   const noexcept { return _data.i; }
    date   as_date()      const noexcept { return _data.d; }
    fp3    as_decimal()   const noexcept { return (is_integer()) ? fp3(_data.i) : _data.f; }
    binop  as_binary_op() const noexcept { return _data.o; }
    block* as_block()     const noexcept { return _data.up_block.get(); }
    list*  as_list()      const noexcept { return _data.up_list.get(); }

    // void set_precomments(unique_ptr<comment_block> up) noexcept { _up_precomments = std::move(up); }
    // void set_postcomment(char* str) noexcept { _postcomment = str; }

    /* convenience equality operator overloads */
    bool operator==(const char* s)        const noexcept { return is_string() && strcmp(as_string(), s) == 0; }
    bool operator==(const std::string& s) const noexcept { return is_string() && s == as_string(); }
    bool operator==(int i)   const noexcept { return is_integer() && as_integer() == i; }
    bool operator==(date d)  const noexcept { return is_date() && as_date() == d; }
    bool operator==(fp3 f)   const noexcept { return is_number() && as_decimal() == f; }
    bool operator==(binop o) const noexcept { return is_binary_op() && as_binary_op() == o; }

    /* inequality operator overloads (OK, do I really have to be this explicit by default, C++?) */
    bool operator!=(const char* arg)        const noexcept { return !(*this == arg); }
    bool operator!=(const std::string& arg) const noexcept { return !(*this == arg); }
    bool operator!=(int arg)   const noexcept { return !(*this == arg); }
    bool operator!=(date arg)  const noexcept { return !(*this == arg); }
    bool operator!=(fp3 arg)   const noexcept { return !(*this == arg); }
    bool operator!=(binop arg) const noexcept { return !(*this == arg); }

    void print(std::ostream&, uint indent = 0) const;
};


/* LIST -- list of N objects */

class list {
    typedef std::vector<object> vec_t;
    vec_t _vec;

public:
    list() = delete;
    list(parser&);

    void print(std::ostream&, uint indent = 0) const;

    object&       operator[](size_t i)       noexcept { return _vec[i]; }
    const object& operator[](size_t i) const noexcept { return _vec[i]; }

    vec_t::size_type      size()  const noexcept { return _vec.size(); }
    bool                  empty() const noexcept { return size() == 0; }
    vec_t::iterator       begin()       noexcept { return _vec.begin(); }
    vec_t::iterator       end()         noexcept { return _vec.end(); }
    vec_t::const_iterator begin() const noexcept { return _vec.cbegin(); }
    vec_t::const_iterator end()   const noexcept { return _vec.cend(); }
};


/* STATEMENT -- statements are pairs of objects and an operator/separator */

class statement {
    object _k;
    object _op;
    object _v;

public:
    statement() = delete;
    statement(object& k, object& op, object& v) : _k(std::move(k)), _op(std::move(op)), _v(std::move(v)) {}

    object const& key()   const noexcept { return _k; }
    object const& op()    const noexcept { return _op; }
    object const& value() const noexcept { return _v; }

    void print(std::ostream&, uint indent = 0) const;
};


/* BLOCK -- blocks contain N statements */

class block {
    /* we maintain two data structures for different ways of efficiently accessing
     * the statements in a block. a linear vector is the master data structure;
     * it actually owns the statement objects, which means that when it is
     * destroyed, so too are any memory resources associated with the objects
     * in its statements.
     *
     * the secondary data structure fulfills a more specialized use case: it maps
     * LHS string-type keys to an index into the statement vector to which the
     * key corresponds, if such a key occurs in this block. if it can occur
     * multiple times, then this block was not constructed via the RHS-merge
     * parsing method (i.e., folderization), and this access pattern may be a
     * lot less helpful. in such cases, the final occurrence of the key in
     * this block will be stored in the hash-map.
     *
     * string-type keys are the only type of keys for which I've encountered a
     * realistic use case for the hash-map access pattern, so rather than create
     * a generalized scalar any-type (like ck2::object but only for scalar data
     * types, or more generally, copy-constructible data types) and hash-map via
     * that any-type, I've simply chosen to stick to string keys for now.
     */

    // linear vector of statements
    typedef std::vector<statement> vec_t;
    vec_t _vec;

    // hash-map of LHS keys to their corresponding statement's index in _vec
    std::unordered_map<cstr, vec_t::difference_type> _map;

public:
    block() { }
    block(parser&, bool is_root = false, bool is_save = false);

    void print(std::ostream&, uint indent = 0) const;

    vec_t::size_type      size()  const noexcept { return _vec.size(); }
    bool                  empty() const noexcept { return size() == 0; }
    vec_t::iterator       begin()       noexcept { return _vec.begin(); }
    vec_t::iterator       end()         noexcept { return _vec.end(); }
    vec_t::const_iterator begin() const noexcept { return _vec.cbegin(); }
    vec_t::const_iterator end()   const noexcept { return _vec.cend(); }

    /* map accessor for statements by LHS statement key (if string-type)
     * if not found, returns this object's end iterator.
     * if found, returns a valid iterator which can be dereferenced. */
    vec_t::iterator find_key(const char* key) noexcept {
        auto i = _map.find(key);
        return (i != _map.end()) ? std::next(begin(), i->second) : end();
    }

    vec_t::const_iterator find_key(const char* key) const noexcept {
        auto i = _map.find(key);
        return (i != _map.end()) ? std::next(begin(), i->second) : end();
    }
};


/* PARSER -- construct a parse tree from a file whose resources are owned by the parser object */

class parser {
protected:
    /* friends only for access to our cstr_pool AFAIK */
    friend class block;
    friend class list;

    lexer _lex;
    cstr_pool<char> _string_pool;
    unique_ptr<block> _up_root_block;
    error_queue _errors;

    char* strdup(const char* s) { return _string_pool.strdup(s); }

    static const uint NUM_LOOKAHEAD_TOKENS = 1;
    // actual token queue size is +1 for the "freebie" lookahead token (the next/current token) and +1 for a "spare"
    // token object which is necessary to guard against dequeuing a token & then peeking past the end of the filled
    // portion of the lookahead queue, which would then overwrite the token text buffer contents of the dequeued token
    // as the queue was refilled and would thus otherwise be an easy possible source of error and generally
    // inconvenient.
    static const uint TQ_SZ = NUM_LOOKAHEAD_TOKENS + 2;
    static const uint TEXT_MAX_SZ = 512; // size of preallocated token text buffers (511 max length)

    bool  _tq_done; // have we read everything from the input stream?
    uint  _tq_head_idx; // index of head of token queue (i.e., next to be handed to user)
    uint  _tq_n; // number of tokens actually in the queue (i.e., that have not been consumed and have been enqueued)
    token _tq[TQ_SZ];
    char  _tq_text[TQ_SZ][TEXT_MAX_SZ]; // text buffers for saving tokens from input scanner; parallel to _tq

    void enqueue_token() {
        uint slot = (_tq_head_idx + _tq_n++) % TQ_SZ;
        _tq_done = !( _lex.read_token_into(_tq[slot], TEXT_MAX_SZ) );
    }

    // fill the token queue such that its effective size is at least `sz`. returns false if that couldn't be satisfied.
    // preconditions: _tq_n <= sz < TQ_SZ
    bool fill_token_queue(uint sz) {
        for (uint needed = sz - _tq_n; needed > 0 && !_tq_done; --needed)
            enqueue_token();
        return _tq_n == sz;
    }

    bool next(token*, bool eof_ok = false);
    void next_expected(token*, uint type);
    void unexpected_token(const token&) const;

    // peek into the token lookahead queue by logical index, POS, of token. position 0 is simply the next token were we
    // to call next(...), and position 1 would be second to next in the input stream (the 1st lookahead token), and so
    // on. if the input stream has ended (EOF, unmatched text) sometime before the lookahead token at which we're
    // peeking, we'll return a nullptr.
    template<const uint POS>
    token* peek() noexcept {
        static_assert(POS <= NUM_LOOKAHEAD_TOKENS, "cannot peek at position greater than parser's number of lookahead tokens");
        return (POS >= _tq_n && !fill_token_queue(POS + 1)) ? nullptr : &_tq[ (_tq_head_idx + POS) % TQ_SZ ];
    }

public:
    parser() = delete;
    parser(const std::string& p, bool is_save = false) : parser(p.c_str(), is_save) {}
    parser(const fs::path& p, bool is_save = false) : parser(p.string().c_str(), is_save) {}
    parser(const char* p, bool is_save = false) : _lex(p), _tq_done(false), _tq_head_idx(0), _tq_n(0) {
        // hook our preallocated token text buffers into the lookahead queue
        for (uint i = 0; i < TQ_SZ; ++i) {
            _tq_text[i][0] = '\0';
            _tq[i].text(_tq_text[i], 0);
        }

        // a parser is nothing without its implicit root block for the file it represents: here, real men get work done.
        _up_root_block = std::make_unique<block>(*this, true, is_save);
    }

    block* root_block() noexcept { return _up_root_block.get(); }
    error_queue& errors() noexcept { return _errors; }
};


/* MISC. UTILITY */

static const uint TIER_BARON   = 1;
static const uint TIER_COUNT   = 2;
static const uint TIER_DUKE    = 3;
static const uint TIER_KING    = 4;
static const uint TIER_EMPEROR = 5;

inline uint title_tier(const char* s) {
    switch (*s) {
    case 'b':
        return TIER_BARON;
    case 'c':
        return TIER_COUNT;
    case 'd':
        return TIER_DUKE;
    case 'k':
        return TIER_KING;
    case 'e':
        return TIER_EMPEROR;
    default:
        return 0;
    }
}

bool looks_like_title(const char*);


_CK2_NAMESPACE_END;


inline std::ostream& operator<<(std::ostream& os, const ck2::block& a) { a.print(os); return os; }
inline std::ostream& operator<<(std::ostream& os, const ck2::list& a) { a.print(os); return os; }
inline std::ostream& operator<<(std::ostream& os, const ck2::statement& a) { a.print(os); return os; }
inline std::ostream& operator<<(std::ostream& os, const ck2::object& a) { a.print(os); return os; }