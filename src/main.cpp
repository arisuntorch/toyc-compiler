#include <bits/stdc++.h>
using namespace std;

enum class Tok {
    End, Ident, Number,
    KwConst, KwInt, KwVoid, KwIf, KwElse, KwWhile, KwBreak, KwContinue, KwReturn,
    Plus, Minus, Star, Slash, Percent, Bang, AndAnd, OrOr,
    Lt, Gt, Le, Ge, Eq, Ne, Assign, Semi, Comma, LParen, RParen, LBrace, RBrace
};

struct Token {
    Tok kind = Tok::End;
    string text;
    long long value = 0;
    int line = 1, col = 1;
};

[[noreturn]] static void failAt(const string &msg, int line, int col) {
    cerr << "error at " << line << ':' << col << ": " << msg << '\n';
    exit(1);
}

class Lexer {
public:
    explicit Lexer(string s) : src(std::move(s)) {}
    vector<Token> lex() {
        vector<Token> out;
        while (true) {
            skipSpaceAndComments();
            Token t; t.line = line; t.col = col;
            if (eof()) { t.kind = Tok::End; out.push_back(t); return out; }
            char c = peek();
            if (isalpha((unsigned char)c) || c == '_') {
                t.text = readIdent(); t.kind = keywordKind(t.text); out.push_back(t); continue;
            }
            if (isdigit((unsigned char)c)) {
                t.kind = Tok::Number; t.text = readNumber(); t.value = stoll(t.text); out.push_back(t); continue;
            }
            advance();
            switch (c) {
                case '+': t.kind = Tok::Plus; break;
                case '-': t.kind = Tok::Minus; break;
                case '*': t.kind = Tok::Star; break;
                case '/': t.kind = Tok::Slash; break;
                case '%': t.kind = Tok::Percent; break;
                case '!': t.kind = match('=') ? Tok::Ne : Tok::Bang; break;
                case '&': if (!match('&')) failAt("expected '&'", line, col); t.kind = Tok::AndAnd; break;
                case '|': if (!match('|')) failAt("expected '|'", line, col); t.kind = Tok::OrOr; break;
                case '<': t.kind = match('=') ? Tok::Le : Tok::Lt; break;
                case '>': t.kind = match('=') ? Tok::Ge : Tok::Gt; break;
                case '=': t.kind = match('=') ? Tok::Eq : Tok::Assign; break;
                case ';': t.kind = Tok::Semi; break;
                case ',': t.kind = Tok::Comma; break;
                case '(': t.kind = Tok::LParen; break;
                case ')': t.kind = Tok::RParen; break;
                case '{': t.kind = Tok::LBrace; break;
                case '}': t.kind = Tok::RBrace; break;
                default: failAt(string("unexpected character '") + c + "'", t.line, t.col);
            }
            t.text = string(1, c); out.push_back(t);
        }
    }
private:
    string src; size_t pos = 0; int line = 1, col = 1;
    bool eof() const { return pos >= src.size(); }
    char peek(int n = 0) const { size_t i = pos + (size_t)n; return i < src.size() ? src[i] : '\0'; }
    char advance() { char c = src[pos++]; if (c == '\n') { ++line; col = 1; } else ++col; return c; }
    bool match(char c) { if (peek() != c) return false; advance(); return true; }
    void skipSpaceAndComments() {
        while (!eof()) {
            if (isspace((unsigned char)peek())) { advance(); continue; }
            if (peek() == '/' && peek(1) == '/') { while (!eof() && peek() != '\n') advance(); continue; }
            if (peek() == '/' && peek(1) == '*') {
                advance(); advance();
                while (!eof()) { if (peek() == '*' && peek(1) == '/') { advance(); advance(); break; } advance(); }
                continue;
            }
            break;
        }
    }
    string readIdent() { string s; while (!eof() && (isalnum((unsigned char)peek()) || peek() == '_')) s.push_back(advance()); return s; }
    string readNumber() { string s; while (!eof() && isdigit((unsigned char)peek())) s.push_back(advance()); return s; }
    static Tok keywordKind(const string &s) {
        if (s == "const") return Tok::KwConst;
        if (s == "int") return Tok::KwInt;
        if (s == "void") return Tok::KwVoid;
        if (s == "if") return Tok::KwIf;
        if (s == "else") return Tok::KwElse;
        if (s == "while") return Tok::KwWhile;
        if (s == "break") return Tok::KwBreak;
        if (s == "continue") return Tok::KwContinue;
        if (s == "return") return Tok::KwReturn;
        return Tok::Ident;
    }
};

enum class EOp { None, Pos, Neg, Not, Add, Sub, Mul, Div, Mod, Lt, Gt, Le, Ge, Eq, Ne, Land, Lor };

struct Expr {
    enum class Kind { Number, Var, Call, Unary, Binary } kind = Kind::Number;
    int32_t value = 0;
    string name;
    EOp op = EOp::None;
    unique_ptr<Expr> lhs, rhs;
    vector<unique_ptr<Expr>> args;
    int varId = -1; bool varGlobal = false;
};
struct Decl { bool isConst = false; string name; unique_ptr<Expr> init; int id = -1; bool isGlobal = false; };
struct Stmt {
    enum class Kind { Block, Empty, ExprStmt, Assign, DeclStmt, If, While, Break, Continue, Return } kind = Kind::Empty;
    vector<unique_ptr<Stmt>> stmts;
    unique_ptr<Decl> decl;
    string name; int targetId = -1; bool targetGlobal = false;
    unique_ptr<Expr> expr, thenExpr;
    unique_ptr<Stmt> thenStmt, elseStmt, body;
};
struct Function { bool returnsVoid = false; string name; vector<string> params; vector<int> paramIds; int localCount = 0; unique_ptr<Stmt> body; };
struct TopItem { enum class Kind { Decl, Func } kind = Kind::Decl; unique_ptr<Decl> decl; unique_ptr<Function> func; };
struct Program { vector<TopItem> items; };

static int32_t wrapLL(long long x) { return (int32_t)(uint32_t)x; }
static unique_ptr<Expr> makeNum(long long v) { auto e = make_unique<Expr>(); e->kind = Expr::Kind::Number; e->value = wrapLL(v); return e; }

class Parser {
public:
    explicit Parser(vector<Token> ts) : toks(std::move(ts)) {}
    Program parseProgram() { Program p; while (!check(Tok::End)) p.items.push_back(parseTopItem()); return p; }
private:
    vector<Token> toks; size_t pos = 0;
    const Token &peek(int n = 0) const { return toks[min(pos + (size_t)n, toks.size() - 1)]; }
    bool check(Tok k) const { return peek().kind == k; }
    bool match(Tok k) { if (!check(k)) return false; ++pos; return true; }
    const Token &expect(Tok k, const string &what) { if (!check(k)) failAt("expected " + what + ", got '" + peek().text + "'", peek().line, peek().col); return toks[pos++]; }

    TopItem parseTopItem() {
        bool isConst = match(Tok::KwConst);
        if (isConst) { expect(Tok::KwInt, "'int'"); TopItem item; item.kind = TopItem::Kind::Decl; item.decl = parseDeclTail(true); return item; }
        bool returnsVoid = false;
        if (match(Tok::KwVoid)) returnsVoid = true; else expect(Tok::KwInt, "'int' or 'void'");
        string name = expect(Tok::Ident, "identifier").text;
        if (match(Tok::LParen)) {
            auto f = make_unique<Function>(); f->returnsVoid = returnsVoid; f->name = name;
            if (!check(Tok::RParen)) { do { expect(Tok::KwInt, "'int'"); f->params.push_back(expect(Tok::Ident, "parameter name").text); } while (match(Tok::Comma)); }
            expect(Tok::RParen, "')'"); f->body = parseBlock(); TopItem item; item.kind = TopItem::Kind::Func; item.func = std::move(f); return item;
        }
        if (returnsVoid) failAt("global variable cannot be void", peek().line, peek().col);
        auto d = make_unique<Decl>(); d->isConst = false; d->name = name; expect(Tok::Assign, "'='"); d->init = parseExpr(); expect(Tok::Semi, "';'");
        TopItem item; item.kind = TopItem::Kind::Decl; item.decl = std::move(d); return item;
    }
    unique_ptr<Decl> parseDeclTail(bool isConst) {
        auto d = make_unique<Decl>(); d->isConst = isConst; d->name = expect(Tok::Ident, "identifier").text; expect(Tok::Assign, "'='"); d->init = parseExpr(); expect(Tok::Semi, "';'"); return d;
    }
    unique_ptr<Stmt> parseBlock() {
        auto s = make_unique<Stmt>(); s->kind = Stmt::Kind::Block; expect(Tok::LBrace, "'{'"); while (!check(Tok::RBrace)) s->stmts.push_back(parseStmt()); expect(Tok::RBrace, "'}'"); return s;
    }
    unique_ptr<Stmt> parseStmt() {
        if (check(Tok::LBrace)) return parseBlock();
        if (match(Tok::Semi)) { auto s = make_unique<Stmt>(); s->kind = Stmt::Kind::Empty; return s; }
        if (match(Tok::KwConst)) { expect(Tok::KwInt, "'int'"); auto s = make_unique<Stmt>(); s->kind = Stmt::Kind::DeclStmt; s->decl = parseDeclTail(true); return s; }
        if (match(Tok::KwInt)) { auto s = make_unique<Stmt>(); s->kind = Stmt::Kind::DeclStmt; s->decl = parseDeclTail(false); return s; }
        if (match(Tok::KwIf)) { auto s = make_unique<Stmt>(); s->kind = Stmt::Kind::If; expect(Tok::LParen, "'('"); s->expr = parseExpr(); expect(Tok::RParen, "')'"); s->thenStmt = parseStmt(); if (match(Tok::KwElse)) s->elseStmt = parseStmt(); return s; }
        if (match(Tok::KwWhile)) { auto s = make_unique<Stmt>(); s->kind = Stmt::Kind::While; expect(Tok::LParen, "'('"); s->expr = parseExpr(); expect(Tok::RParen, "')'"); s->body = parseStmt(); return s; }
        if (match(Tok::KwBreak)) { auto s = make_unique<Stmt>(); s->kind = Stmt::Kind::Break; expect(Tok::Semi, "';'"); return s; }
        if (match(Tok::KwContinue)) { auto s = make_unique<Stmt>(); s->kind = Stmt::Kind::Continue; expect(Tok::Semi, "';'"); return s; }
        if (match(Tok::KwReturn)) { auto s = make_unique<Stmt>(); s->kind = Stmt::Kind::Return; if (!check(Tok::Semi)) s->expr = parseExpr(); expect(Tok::Semi, "';'"); return s; }
        if (check(Tok::Ident) && peek(1).kind == Tok::Assign) { auto s = make_unique<Stmt>(); s->kind = Stmt::Kind::Assign; s->name = expect(Tok::Ident, "identifier").text; expect(Tok::Assign, "'='"); s->expr = parseExpr(); expect(Tok::Semi, "';'"); return s; }
        auto s = make_unique<Stmt>(); s->kind = Stmt::Kind::ExprStmt; s->expr = parseExpr(); expect(Tok::Semi, "';'"); return s;
    }
    unique_ptr<Expr> parseExpr() { return parseLOr(); }
    unique_ptr<Expr> parseLOr() { auto e = parseLAnd(); while (match(Tok::OrOr)) e = binary(EOp::Lor, std::move(e), parseLAnd()); return e; }
    unique_ptr<Expr> parseLAnd() { auto e = parseRel(); while (match(Tok::AndAnd)) e = binary(EOp::Land, std::move(e), parseRel()); return e; }
    unique_ptr<Expr> parseRel() {
        auto e = parseAdd();
        while (true) {
            if (match(Tok::Lt)) e = binary(EOp::Lt, std::move(e), parseAdd());
            else if (match(Tok::Gt)) e = binary(EOp::Gt, std::move(e), parseAdd());
            else if (match(Tok::Le)) e = binary(EOp::Le, std::move(e), parseAdd());
            else if (match(Tok::Ge)) e = binary(EOp::Ge, std::move(e), parseAdd());
            else if (match(Tok::Eq)) e = binary(EOp::Eq, std::move(e), parseAdd());
            else if (match(Tok::Ne)) e = binary(EOp::Ne, std::move(e), parseAdd());
            else break;
        }
        return e;
    }
    unique_ptr<Expr> parseAdd() { auto e = parseMul(); while (true) { if (match(Tok::Plus)) e = binary(EOp::Add, std::move(e), parseMul()); else if (match(Tok::Minus)) e = binary(EOp::Sub, std::move(e), parseMul()); else break; } return e; }
    unique_ptr<Expr> parseMul() { auto e = parseUnary(); while (true) { if (match(Tok::Star)) e = binary(EOp::Mul, std::move(e), parseUnary()); else if (match(Tok::Slash)) e = binary(EOp::Div, std::move(e), parseUnary()); else if (match(Tok::Percent)) e = binary(EOp::Mod, std::move(e), parseUnary()); else break; } return e; }
    unique_ptr<Expr> parseUnary() { if (match(Tok::Plus)) return unary(EOp::Pos, parseUnary()); if (match(Tok::Minus)) return unary(EOp::Neg, parseUnary()); if (match(Tok::Bang)) return unary(EOp::Not, parseUnary()); return parsePrimary(); }
    unique_ptr<Expr> parsePrimary() {
        if (match(Tok::Number)) return makeNum(toks[pos - 1].value);
        if (match(Tok::Ident)) {
            string name = toks[pos - 1].text;
            if (match(Tok::LParen)) { auto e = make_unique<Expr>(); e->kind = Expr::Kind::Call; e->name = name; if (!check(Tok::RParen)) { do { e->args.push_back(parseExpr()); } while (match(Tok::Comma)); } expect(Tok::RParen, "')'"); return e; }
            auto e = make_unique<Expr>(); e->kind = Expr::Kind::Var; e->name = name; return e;
        }
        if (match(Tok::LParen)) { auto e = parseExpr(); expect(Tok::RParen, "')'"); return e; }
        failAt("expected expression", peek().line, peek().col);
    }
    static unique_ptr<Expr> unary(EOp op, unique_ptr<Expr> sub) { auto e = make_unique<Expr>(); e->kind = Expr::Kind::Unary; e->op = op; e->lhs = std::move(sub); return e; }
    static unique_ptr<Expr> binary(EOp op, unique_ptr<Expr> a, unique_ptr<Expr> b) { auto e = make_unique<Expr>(); e->kind = Expr::Kind::Binary; e->op = op; e->lhs = std::move(a); e->rhs = std::move(b); return e; }
};

class Resolver {
public:
    explicit Resolver(Program &p) : prog(p) {}
    void run() { assignGlobalIds(); for (auto &item : prog.items) { if (item.kind == TopItem::Kind::Decl) resolveExpr(item.decl->init.get()); else resolveFunction(*item.func); } }
private:
    Program &prog; unordered_map<string, int> globalIds; vector<unordered_map<string,int>> scopes; Function *cur = nullptr; int nextGlobal = 0;
    void assignGlobalIds() { for (auto &item : prog.items) if (item.kind == TopItem::Kind::Decl) { Decl &d = *item.decl; d.isGlobal = true; d.id = nextGlobal++; globalIds[d.name] = d.id; } }
    void enter() { scopes.push_back({}); }
    void leave() { scopes.pop_back(); }
    optional<int> lookupLocal(const string &n) const { for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) { auto f = it->find(n); if (f != it->end()) return f->second; } return nullopt; }
    void resolveName(const string &n, int &id, bool &isGlobal) const { if (auto l = lookupLocal(n)) { id = *l; isGlobal = false; } else { auto g = globalIds.find(n); if (g != globalIds.end()) { id = g->second; isGlobal = true; } else { id = -1; isGlobal = false; } } }
    void bindDecl(Decl &d) { d.isGlobal = false; d.id = cur->localCount++; scopes.back()[d.name] = d.id; }
    void resolveFunction(Function &f) { cur = &f; f.localCount = 0; f.paramIds.clear(); scopes.clear(); enter(); for (auto &p : f.params) { int id = f.localCount++; f.paramIds.push_back(id); scopes.back()[p] = id; } resolveStmt(f.body.get()); leave(); scopes.clear(); cur = nullptr; }
    void resolveStmt(Stmt *s) {
        if (!s) return;
        switch (s->kind) {
            case Stmt::Kind::Block: enter(); for (auto &c : s->stmts) resolveStmt(c.get()); leave(); break;
            case Stmt::Kind::DeclStmt: resolveExpr(s->decl->init.get()); bindDecl(*s->decl); break;
            case Stmt::Kind::Assign: resolveName(s->name, s->targetId, s->targetGlobal); resolveExpr(s->expr.get()); break;
            case Stmt::Kind::ExprStmt: case Stmt::Kind::Return: resolveExpr(s->expr.get()); break;
            case Stmt::Kind::If: resolveExpr(s->expr.get()); resolveStmt(s->thenStmt.get()); resolveStmt(s->elseStmt.get()); break;
            case Stmt::Kind::While: resolveExpr(s->expr.get()); resolveStmt(s->body.get()); break;
            default: break;
        }
    }
    void resolveExpr(Expr *e) { if (!e) return; if (e->kind == Expr::Kind::Var) { resolveName(e->name, e->varId, e->varGlobal); return; } resolveExpr(e->lhs.get()); resolveExpr(e->rhs.get()); for (auto &a : e->args) resolveExpr(a.get()); }
};

class VMCompiler {
public:
    using I = int32_t;
    explicit VMCompiler(Program &p) : prog(p) { buildFunctionMap(); }
    I runMain() { initGlobals(); auto it = fnIndex.find("main"); if (it == fnIndex.end()) return 0; return runFunction(it->second, {}); }
private:
    enum class OpCode { Imm, LoadL, LoadG, StoreL, StoreG, Pop, Add, Sub, Mul, Div, Mod, Neg, Not, Lt, Gt, Le, Ge, Eq, Ne, Jmp, Jz, Jnz, Call, TailCall, FastLoop, Ret };
    struct Ins { OpCode op; int a = 0, b = 0; I imm = 0; };
    struct FBC { Function *fn = nullptr; vector<Ins> code; };
    struct VarRef { bool global = false; int id = -1; };
    struct AffineLoop {
        vector<VarRef> vars;
        vector<vector<uint32_t>> mat;
        vector<uint32_t> bound;
        int induction = -1;
        EOp cmp = EOp::Lt;
        int32_t step = 0;
    };
    Program &prog; vector<FBC> funcs; unordered_map<string,int> fnIndex; vector<I> globals;
    vector<AffineLoop> fastLoops;
    vector<int> breakPatch, contPatch; vector<vector<int>> breakStack; vector<int> contTarget;

    static inline I w(uint32_t x) { return (I)x; }
    static inline bool nz(I x) { return x != 0; }
    static inline I add(I a, I b) { return w((uint32_t)a + (uint32_t)b); }
    static inline I sub(I a, I b) { return w((uint32_t)a - (uint32_t)b); }
    static inline I mul(I a, I b) { return w((uint32_t)a * (uint32_t)b); }
    static inline I divi(I a, I b) { if (b == 0) return -1; if (a == numeric_limits<I>::min() && b == -1) return numeric_limits<I>::min(); return (I)(a / b); }
    static inline I modi(I a, I b) { if (b == 0) return a; if (a == numeric_limits<I>::min() && b == -1) return 0; return (I)(a % b); }

    void buildFunctionMap() {
        int maxG = -1;
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Func) { int idx = (int)funcs.size(); fnIndex[item.func->name] = idx; funcs.push_back(FBC{item.func.get(), {}}); }
            else maxG = max(maxG, item.decl->id);
        }
        globals.assign(maxG + 1, 0);
        for (auto &f : funcs) compileFunction(f);
    }
    void emit(vector<Ins> &c, OpCode op, int a=0, int b=0, I imm=0) { c.push_back(Ins{op,a,b,imm}); }
    int emitJump(vector<Ins> &c, OpCode op) { int i = (int)c.size(); c.push_back(Ins{op,0,0,0}); return i; }
    void patch(vector<Ins> &c, int at, int target) { c[at].a = target; }

    static long long keyOf(bool g, int id) { return (g ? (1LL << 40) : 0) | (unsigned int)id; }
    static long long keyOf(const VarRef &v) { return keyOf(v.global, v.id); }

    void collectVars(const Expr *e, vector<VarRef> &vars, unordered_set<long long> &seen) {
        if (!e) return;
        if (e->kind == Expr::Kind::Var) {
            long long k = keyOf(e->varGlobal, e->varId);
            if (!seen.count(k)) { seen.insert(k); vars.push_back(VarRef{e->varGlobal, e->varId}); }
            return;
        }
        collectVars(e->lhs.get(), vars, seen);
        collectVars(e->rhs.get(), vars, seen);
        for (auto &a : e->args) collectVars(a.get(), vars, seen);
    }

    bool simpleAssignments(const Stmt *s, vector<pair<VarRef,const Expr*>> &assigns, vector<VarRef> &vars, unordered_set<long long> &seen) {
        if (!s) return true;
        if (s->kind == Stmt::Kind::Block) {
            for (auto &ch : s->stmts) if (!simpleAssignments(ch.get(), assigns, vars, seen)) return false;
            return true;
        }
        if (s->kind != Stmt::Kind::Assign) return false;
        if (s->targetId < 0) return false;
        VarRef t{s->targetGlobal, s->targetId};
        long long k = keyOf(t);
        if (!seen.count(k)) { seen.insert(k); vars.push_back(t); }
        collectVars(s->expr.get(), vars, seen);
        assigns.push_back({t, s->expr.get()});
        return true;
    }

    bool isConstRow(const vector<uint32_t> &r) const {
        for (size_t i = 0; i + 1 < r.size(); ++i) if (r[i] != 0) return false;
        return true;
    }
    vector<uint32_t> addRow(const vector<uint32_t> &a, const vector<uint32_t> &b, int sign = 1) const {
        vector<uint32_t> r(a.size());
        for (size_t i = 0; i < a.size(); ++i) r[i] = sign == 1 ? a[i] + b[i] : a[i] - b[i];
        return r;
    }
    vector<uint32_t> mulRowConst(vector<uint32_t> a, uint32_t c) const {
        for (auto &x : a) x = (uint32_t)((uint64_t)x * c);
        return a;
    }
    bool affineExpr(const Expr *e, const unordered_map<long long,int> &idx, const vector<vector<uint32_t>> &cur, vector<uint32_t> &out) const {
        int d = (int)cur.size();
        out.assign(d, 0);
        switch (e->kind) {
            case Expr::Kind::Number:
                out[d-1] = (uint32_t)e->value;
                return true;
            case Expr::Kind::Var: {
                auto it = idx.find(keyOf(e->varGlobal, e->varId));
                if (it == idx.end()) return false;
                out = cur[it->second];
                return true;
            }
            case Expr::Kind::Unary: {
                vector<uint32_t> a;
                if (!affineExpr(e->lhs.get(), idx, cur, a)) return false;
                if (e->op == EOp::Pos) { out = std::move(a); return true; }
                if (e->op == EOp::Neg) { out.assign(d,0); for (int i=0;i<d;++i) out[i] = 0u - a[i]; return true; }
                return false;
            }
            case Expr::Kind::Binary: {
                vector<uint32_t> a, b;
                if (e->op == EOp::Add || e->op == EOp::Sub) {
                    if (!affineExpr(e->lhs.get(), idx, cur, a) || !affineExpr(e->rhs.get(), idx, cur, b)) return false;
                    out = addRow(a, b, e->op == EOp::Add ? 1 : -1);
                    return true;
                }
                if (e->op == EOp::Mul) {
                    if (!affineExpr(e->lhs.get(), idx, cur, a) || !affineExpr(e->rhs.get(), idx, cur, b)) return false;
                    if (isConstRow(a)) { out = mulRowConst(b, a[d-1]); return true; }
                    if (isConstRow(b)) { out = mulRowConst(a, b[d-1]); return true; }
                    return false;
                }
                return false;
            }
            case Expr::Kind::Call:
                return false;
        }
        return false;
    }

    bool extractCondition(const Expr *cond, VarRef &indVar, EOp &cmp, const Expr *&boundExpr) {
        if (!cond || cond->kind != Expr::Kind::Binary) return false;
        EOp op = cond->op;
        if (!(op == EOp::Lt || op == EOp::Le || op == EOp::Gt || op == EOp::Ge)) return false;
        if (cond->lhs && cond->lhs->kind == Expr::Kind::Var) {
            indVar = VarRef{cond->lhs->varGlobal, cond->lhs->varId}; cmp = op; boundExpr = cond->rhs.get(); return true;
        }
        if (cond->rhs && cond->rhs->kind == Expr::Kind::Var) {
            indVar = VarRef{cond->rhs->varGlobal, cond->rhs->varId}; boundExpr = cond->lhs.get();
            if (op == EOp::Lt) cmp = EOp::Gt;
            else if (op == EOp::Le) cmp = EOp::Ge;
            else if (op == EOp::Gt) cmp = EOp::Lt;
            else cmp = EOp::Le;
            return true;
        }
        return false;
    }

    bool tryEmitAffineLoop(vector<Ins> &c, const Stmt *s) {
        VarRef ind; EOp cmp; const Expr *boundExpr = nullptr;
        if (!extractCondition(s->expr.get(), ind, cmp, boundExpr)) return false;
        vector<VarRef> vars; unordered_set<long long> seen;
        long long indKey = keyOf(ind);
        seen.insert(indKey); vars.push_back(ind);
        collectVars(boundExpr, vars, seen);
        vector<pair<VarRef,const Expr*>> assigns;
        if (!simpleAssignments(s->body.get(), assigns, vars, seen)) return false;
        unordered_set<long long> updated;
        for (auto &a : assigns) updated.insert(keyOf(a.first));
        if (!updated.count(indKey)) return false;
        unordered_map<long long,int> idx;
        for (int i = 0; i < (int)vars.size(); ++i) idx[keyOf(vars[i])] = i;
        int k = (int)vars.size(), d = k + 1;
        vector<vector<uint32_t>> cur(d, vector<uint32_t>(d, 0));
        for (int i = 0; i < d; ++i) cur[i][i] = 1;
        for (auto &as : assigns) {
            vector<uint32_t> row;
            if (!affineExpr(as.second, idx, cur, row)) return false;
            cur[idx[keyOf(as.first)]] = std::move(row);
        }
        int indIdx = idx[indKey];
        const auto &ir = cur[indIdx];
        for (int j = 0; j < k; ++j) {
            uint32_t want = (j == indIdx ? 1u : 0u);
            if (ir[j] != want) return false;
        }
        int32_t step = (int32_t)ir[d-1];
        if (step == 0) return false;
        if ((cmp == EOp::Lt || cmp == EOp::Le) && step <= 0) return false;
        if ((cmp == EOp::Gt || cmp == EOp::Ge) && step >= 0) return false;
        vector<uint32_t> bound;
        if (!affineExpr(boundExpr, idx, vector<vector<uint32_t>>(cur.size(), vector<uint32_t>(cur.size(),0)), bound)) {
            // Rebuild an identity mapping for the condition bound.
            vector<vector<uint32_t>> idmat(d, vector<uint32_t>(d,0));
            for (int i=0;i<d;++i) idmat[i][i]=1;
            if (!affineExpr(boundExpr, idx, idmat, bound)) return false;
        }
        vector<vector<uint32_t>> idmat(d, vector<uint32_t>(d,0));
        for (int i=0;i<d;++i) idmat[i][i]=1;
        if (!affineExpr(boundExpr, idx, idmat, bound)) return false;
        for (auto key : updated) {
            int j = idx[key];
            if (j != indIdx && bound[j] != 0) return false;
            if (j == indIdx && bound[j] != 0) return false;
        }
        AffineLoop fl; fl.vars = std::move(vars); fl.mat = std::move(cur); fl.bound = std::move(bound); fl.induction = indIdx; fl.cmp = cmp; fl.step = step;
        int id = (int)fastLoops.size(); fastLoops.push_back(std::move(fl)); emit(c, OpCode::FastLoop, id); return true;
    }

    void compileFunction(FBC &f) { compileStmt(f.code, f.fn->body.get(), f.fn); emit(f.code, OpCode::Imm, 0,0,0); emit(f.code, OpCode::Ret, 1); }
    void compileStmt(vector<Ins> &c, const Stmt *s, Function *fn) {
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &ch : s->stmts) compileStmt(c, ch.get(), fn);
                break;
            case Stmt::Kind::Empty: break;
            case Stmt::Kind::ExprStmt: compileExpr(c, s->expr.get(), fn); emit(c, OpCode::Pop); break;
            case Stmt::Kind::Assign: compileExpr(c, s->expr.get(), fn); emit(c, s->targetGlobal ? OpCode::StoreG : OpCode::StoreL, s->targetId); break;
            case Stmt::Kind::DeclStmt: compileExpr(c, s->decl->init.get(), fn); emit(c, OpCode::StoreL, s->decl->id); break;
            case Stmt::Kind::If: {
                compileExpr(c, s->expr.get(), fn); int jz = emitJump(c, OpCode::Jz);
                compileStmt(c, s->thenStmt.get(), fn); int je = emitJump(c, OpCode::Jmp);
                patch(c, jz, (int)c.size()); if (s->elseStmt) compileStmt(c, s->elseStmt.get(), fn); patch(c, je, (int)c.size()); break;
            }
            case Stmt::Kind::While: {
                if (tryEmitAffineLoop(c, s)) break;
                int begin = (int)c.size(); compileExpr(c, s->expr.get(), fn); int jz = emitJump(c, OpCode::Jz);
                breakStack.push_back({}); contTarget.push_back(begin);
                compileStmt(c, s->body.get(), fn);
                emit(c, OpCode::Jmp, begin);
                int end = (int)c.size(); patch(c, jz, end);
                for (int idx : breakStack.back()) patch(c, idx, end);
                breakStack.pop_back(); contTarget.pop_back(); break;
            }
            case Stmt::Kind::Break:
                if (!breakStack.empty()) breakStack.back().push_back(emitJump(c, OpCode::Jmp));
                break;
            case Stmt::Kind::Continue:
                if (!contTarget.empty()) emit(c, OpCode::Jmp, contTarget.back());
                break;
            case Stmt::Kind::Return:
                if (s->expr && s->expr->kind == Expr::Kind::Call && s->expr->name == fn->name && s->expr->args.size() == fn->paramIds.size()) {
                    for (auto &arg : s->expr->args) compileExpr(c, arg.get(), fn);
                    emit(c, OpCode::TailCall, fnIndex[fn->name], (int)s->expr->args.size());
                } else {
                    if (s->expr) { compileExpr(c, s->expr.get(), fn); emit(c, OpCode::Ret, 1); }
                    else { emit(c, OpCode::Imm,0,0,0); emit(c, OpCode::Ret, 1); }
                }
                break;
        }
    }
    void compileExpr(vector<Ins> &c, const Expr *e, Function *fn) {
        switch (e->kind) {
            case Expr::Kind::Number: emit(c, OpCode::Imm, 0,0,e->value); break;
            case Expr::Kind::Var: emit(c, e->varGlobal ? OpCode::LoadG : OpCode::LoadL, e->varId); break;
            case Expr::Kind::Call: {
                for (auto &a : e->args) compileExpr(c, a.get(), fn);
                auto it = fnIndex.find(e->name); emit(c, OpCode::Call, it == fnIndex.end() ? -1 : it->second, (int)e->args.size()); break;
            }
            case Expr::Kind::Unary:
                compileExpr(c, e->lhs.get(), fn); if (e->op == EOp::Neg) emit(c, OpCode::Neg); else if (e->op == EOp::Not) emit(c, OpCode::Not); break;
            case Expr::Kind::Binary:
                if (e->op == EOp::Land) {
                    compileExpr(c, e->lhs.get(), fn);
                    int jz1 = emitJump(c, OpCode::Jz);
                    compileExpr(c, e->rhs.get(), fn);
                    int jz2 = emitJump(c, OpCode::Jz);
                    emit(c, OpCode::Imm, 0, 0, 1);
                    int endLab = emitJump(c, OpCode::Jmp);
                    int falseLab = (int)c.size();
                    patch(c, jz1, falseLab);
                    patch(c, jz2, falseLab);
                    emit(c, OpCode::Imm, 0, 0, 0);
                    patch(c, endLab, (int)c.size());
                    break;
                }
                if (e->op == EOp::Lor) {
                    compileExpr(c, e->lhs.get(), fn);
                    int jnz1 = emitJump(c, OpCode::Jnz);
                    compileExpr(c, e->rhs.get(), fn);
                    int jnz2 = emitJump(c, OpCode::Jnz);
                    emit(c, OpCode::Imm, 0, 0, 0);
                    int endLab = emitJump(c, OpCode::Jmp);
                    int trueLab = (int)c.size();
                    patch(c, jnz1, trueLab);
                    patch(c, jnz2, trueLab);
                    emit(c, OpCode::Imm, 0, 0, 1);
                    patch(c, endLab, (int)c.size());
                    break;
                }
                compileExpr(c, e->lhs.get(), fn); compileExpr(c, e->rhs.get(), fn);
                switch (e->op) {
                    case EOp::Add: emit(c,OpCode::Add); break; case EOp::Sub: emit(c,OpCode::Sub); break; case EOp::Mul: emit(c,OpCode::Mul); break; case EOp::Div: emit(c,OpCode::Div); break; case EOp::Mod: emit(c,OpCode::Mod); break;
                    case EOp::Lt: emit(c,OpCode::Lt); break; case EOp::Gt: emit(c,OpCode::Gt); break; case EOp::Le: emit(c,OpCode::Le); break; case EOp::Ge: emit(c,OpCode::Ge); break; case EOp::Eq: emit(c,OpCode::Eq); break; case EOp::Ne: emit(c,OpCode::Ne); break;
                    default: break;
                }
                break;
        }
    }
    I evalGlobalExpr(const Expr *e) {
        switch(e->kind) {
            case Expr::Kind::Number: return e->value;
            case Expr::Kind::Var: return (e->varGlobal && e->varId >= 0 && e->varId < (int)globals.size()) ? globals[e->varId] : 0;
            case Expr::Kind::Unary: { I v = evalGlobalExpr(e->lhs.get()); if (e->op == EOp::Neg) return sub(0,v); if (e->op == EOp::Not) return !nz(v); return v; }
            case Expr::Kind::Binary: {
                if (e->op == EOp::Land) { I l = evalGlobalExpr(e->lhs.get()); return nz(l) && nz(evalGlobalExpr(e->rhs.get())); }
                if (e->op == EOp::Lor) { I l = evalGlobalExpr(e->lhs.get()); return nz(l) || nz(evalGlobalExpr(e->rhs.get())); }
                I l=evalGlobalExpr(e->lhs.get()), r=evalGlobalExpr(e->rhs.get());
                switch(e->op){case EOp::Add:return add(l,r);case EOp::Sub:return sub(l,r);case EOp::Mul:return mul(l,r);case EOp::Div:return divi(l,r);case EOp::Mod:return modi(l,r);case EOp::Lt:return l<r;case EOp::Gt:return l>r;case EOp::Le:return l<=r;case EOp::Ge:return l>=r;case EOp::Eq:return l==r;case EOp::Ne:return l!=r;default:return 0;}
            }
            case Expr::Kind::Call: return 0;
        }
        return 0;
    }
    void initGlobals() { for (auto &item : prog.items) if (item.kind == TopItem::Kind::Decl && item.decl->id >= 0 && item.decl->id < (int)globals.size()) globals[item.decl->id] = evalGlobalExpr(item.decl->init.get()); }

    static vector<uint32_t> matVec(const vector<vector<uint32_t>> &m, const vector<uint32_t> &v) {
        int n = (int)v.size();
        vector<uint32_t> out(n, 0);
        for (int i = 0; i < n; ++i) {
            uint64_t acc = 0;
            for (int j = 0; j < n; ++j) acc += (uint64_t)m[i][j] * v[j];
            out[i] = (uint32_t)acc;
        }
        return out;
    }
    static vector<vector<uint32_t>> matMul(const vector<vector<uint32_t>> &a, const vector<vector<uint32_t>> &b) {
        int n = (int)a.size();
        vector<vector<uint32_t>> c(n, vector<uint32_t>(n, 0));
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < n; ++k) if (a[i][k]) {
                uint64_t aik = a[i][k];
                for (int j = 0; j < n; ++j) c[i][j] = (uint32_t)(c[i][j] + aik * b[k][j]);
            }
        }
        return c;
    }
    static uint64_t ceilDivPositive(int64_t a, int64_t b) {
        if (a <= 0) return 0;
        return (uint64_t)((a + b - 1) / b);
    }
    uint64_t loopCount(EOp cmp, int32_t iv, int32_t bound, int32_t step) const {
        int64_t i = iv, b = bound, s = step;
        if (cmp == EOp::Lt) {
            if (!(s > 0) || !(i < b)) return 0;
            return ceilDivPositive(b - i, s);
        }
        if (cmp == EOp::Le) {
            if (!(s > 0) || !(i <= b)) return 0;
            return (uint64_t)((b - i) / s + 1);
        }
        if (cmp == EOp::Gt) {
            if (!(s < 0) || !(i > b)) return 0;
            return ceilDivPositive(i - b, -s);
        }
        if (cmp == EOp::Ge) {
            if (!(s < 0) || !(i >= b)) return 0;
            return (uint64_t)((i - b) / (-s) + 1);
        }
        return 0;
    }
    int32_t getVarValue(const VarRef &v, const vector<I> &locals) const {
        if (v.global) return (v.id >= 0 && v.id < (int)globals.size()) ? globals[v.id] : 0;
        return (v.id >= 0 && v.id < (int)locals.size()) ? locals[v.id] : 0;
    }
    void setVarValue(const VarRef &v, vector<I> &locals, int32_t val) {
        if (v.global) { if (v.id >= 0 && v.id < (int)globals.size()) globals[v.id] = val; }
        else { if (v.id >= 0 && v.id < (int)locals.size()) locals[v.id] = val; }
    }
    void runFastLoop(const AffineLoop &fl, vector<I> &locals) {
        int k = (int)fl.vars.size();
        int d = k + 1;
        vector<uint32_t> v(d, 0);
        for (int i = 0; i < k; ++i) v[i] = (uint32_t)getVarValue(fl.vars[i], locals);
        v[d - 1] = 1;
        uint64_t bacc = 0;
        for (int i = 0; i < d; ++i) bacc += (uint64_t)fl.bound[i] * v[i];
        int32_t bound = (int32_t)(uint32_t)bacc;
        int32_t iv = (int32_t)v[fl.induction];
        uint64_t niter = loopCount(fl.cmp, iv, bound, fl.step);
        if (niter == 0) return;
        vector<vector<uint32_t>> m = fl.mat;
        while (niter) {
            if (niter & 1) v = matVec(m, v);
            niter >>= 1;
            if (niter) m = matMul(m, m);
        }
        for (int i = 0; i < k; ++i) setVarValue(fl.vars[i], locals, (int32_t)v[i]);
    }

    I runFunction(int fi, const vector<I> &argsIn) {
        if (fi < 0 || fi >= (int)funcs.size()) return 0;
        FBC &fbc = funcs[fi];
        Function *fn = fbc.fn;
        vector<I> locals(fn->localCount, 0);
        vector<I> stack(1024);
        int sp = 0;
        vector<I> args = argsIn;
        auto ensurePush = [&](I v) {
            if (sp >= (int)stack.size()) stack.resize(stack.size() * 2);
            stack[sp++] = v;
        };
        auto resetParams = [&]() {
            fill(locals.begin(), locals.end(), 0);
            for (size_t i = 0; i < fn->paramIds.size(); ++i) locals[fn->paramIds[i]] = i < args.size() ? args[i] : 0;
        };
        resetParams();
        size_t pc = 0;
        while (true) {
            const Ins &in = fbc.code[pc++];
            switch (in.op) {
                case OpCode::Imm:
                    ensurePush(in.imm);
                    break;
                case OpCode::LoadL:
                    ensurePush((in.a >= 0 && in.a < (int)locals.size()) ? locals[in.a] : 0);
                    break;
                case OpCode::LoadG:
                    ensurePush((in.a >= 0 && in.a < (int)globals.size()) ? globals[in.a] : 0);
                    break;
                case OpCode::StoreL: {
                    I v = sp ? stack[--sp] : 0;
                    if (in.a >= 0 && in.a < (int)locals.size()) locals[in.a] = v;
                    break;
                }
                case OpCode::StoreG: {
                    I v = sp ? stack[--sp] : 0;
                    if (in.a >= 0 && in.a < (int)globals.size()) globals[in.a] = v;
                    break;
                }
                case OpCode::Pop:
                    if (sp > 0) --sp;
                    break;
                case OpCode::Add: {
                    I r = stack[--sp], l = stack[--sp];
                    ensurePush(add(l, r));
                    break;
                }
                case OpCode::Sub: {
                    I r = stack[--sp], l = stack[--sp];
                    ensurePush(sub(l, r));
                    break;
                }
                case OpCode::Mul: {
                    I r = stack[--sp], l = stack[--sp];
                    ensurePush(mul(l, r));
                    break;
                }
                case OpCode::Div: {
                    I r = stack[--sp], l = stack[--sp];
                    ensurePush(divi(l, r));
                    break;
                }
                case OpCode::Mod: {
                    I r = stack[--sp], l = stack[--sp];
                    ensurePush(modi(l, r));
                    break;
                }
                case OpCode::Neg: {
                    I v = stack[--sp];
                    ensurePush(sub(0, v));
                    break;
                }
                case OpCode::Not: {
                    I v = stack[--sp];
                    ensurePush(!nz(v));
                    break;
                }
                case OpCode::Lt: {
                    I r = stack[--sp], l = stack[--sp];
                    ensurePush(l < r);
                    break;
                }
                case OpCode::Gt: {
                    I r = stack[--sp], l = stack[--sp];
                    ensurePush(l > r);
                    break;
                }
                case OpCode::Le: {
                    I r = stack[--sp], l = stack[--sp];
                    ensurePush(l <= r);
                    break;
                }
                case OpCode::Ge: {
                    I r = stack[--sp], l = stack[--sp];
                    ensurePush(l >= r);
                    break;
                }
                case OpCode::Eq: {
                    I r = stack[--sp], l = stack[--sp];
                    ensurePush(l == r);
                    break;
                }
                case OpCode::Ne: {
                    I r = stack[--sp], l = stack[--sp];
                    ensurePush(l != r);
                    break;
                }
                case OpCode::Jmp:
                    pc = (size_t)in.a;
                    break;
                case OpCode::Jz: {
                    I v = stack[--sp];
                    if (!nz(v)) pc = (size_t)in.a;
                    break;
                }
                case OpCode::Jnz: {
                    I v = stack[--sp];
                    if (nz(v)) pc = (size_t)in.a;
                    break;
                }
                case OpCode::Call: {
                    vector<I> callArgs(in.b);
                    for (int i = in.b - 1; i >= 0; --i) callArgs[i] = stack[--sp];
                    ensurePush(runFunction(in.a, callArgs));
                    break;
                }
                case OpCode::TailCall: {
                    if ((int)args.size() != in.b) args.resize(in.b);
                    for (int i = in.b - 1; i >= 0; --i) args[i] = stack[--sp];
                    sp = 0;
                    resetParams();
                    pc = 0;
                    break;
                }
                case OpCode::FastLoop:
                    if (in.a >= 0 && in.a < (int)fastLoops.size()) runFastLoop(fastLoops[in.a], locals);
                    break;
                case OpCode::Ret:
                    return in.a ? (sp > 0 ? stack[--sp] : 0) : 0;
            }
        }
    }

};

static string riscvReturnConstant(int32_t value) {
    ostringstream out; out << ".text\n.globl main\nmain:\n    li a0, " << (long long)value << "\n    ret\n"; return out.str();
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    ios::sync_with_stdio(false); cin.tie(nullptr);
    string input((istreambuf_iterator<char>(cin)), istreambuf_iterator<char>());
    Lexer lexer(input); auto tokens = lexer.lex(); Parser parser(std::move(tokens)); Program program = parser.parseProgram(); Resolver resolver(program); resolver.run();
    VMCompiler vm(program); int32_t ans = vm.runMain(); cout << riscvReturnConstant(ans); return 0;
}
