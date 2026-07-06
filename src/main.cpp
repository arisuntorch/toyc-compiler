#include <bits/stdc++.h>
using namespace std;

enum class Tok {
    End,
    Ident,
    Number,
    KwConst,
    KwInt,
    KwVoid,
    KwIf,
    KwElse,
    KwWhile,
    KwBreak,
    KwContinue,
    KwReturn,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Bang,
    AndAnd,
    OrOr,
    Lt,
    Gt,
    Le,
    Ge,
    Eq,
    Ne,
    Assign,
    Semi,
    Comma,
    LParen,
    RParen,
    LBrace,
    RBrace
};

struct Token {
    Tok kind;
    string text;
    long long value = 0;
    int line = 1;
    int col = 1;
};

[[noreturn]] static void failAt(const string &msg, int line, int col) {
    cerr << "error at " << line << ":" << col << ": " << msg << "\n";
    exit(1);
}

class Lexer {
public:
    explicit Lexer(string input) : src(std::move(input)) {}

    vector<Token> lex() {
        vector<Token> out;
        while (true) {
            skipSpaceAndComments();
            Token t;
            t.line = line;
            t.col = col;
            if (eof()) {
                t.kind = Tok::End;
                out.push_back(t);
                return out;
            }
            char c = peek();
            if (isalpha(static_cast<unsigned char>(c)) || c == '_') {
                t.text = readIdent();
                t.kind = keywordKind(t.text);
                out.push_back(t);
                continue;
            }
            if (isdigit(static_cast<unsigned char>(c))) {
                t.kind = Tok::Number;
                t.text = readNumber();
                t.value = stoll(t.text);
                out.push_back(t);
                continue;
            }
            advance();
            switch (c) {
                case '+': t.kind = Tok::Plus; break;
                case '-': t.kind = Tok::Minus; break;
                case '*': t.kind = Tok::Star; break;
                case '%': t.kind = Tok::Percent; break;
                case ';': t.kind = Tok::Semi; break;
                case ',': t.kind = Tok::Comma; break;
                case '(': t.kind = Tok::LParen; break;
                case ')': t.kind = Tok::RParen; break;
                case '{': t.kind = Tok::LBrace; break;
                case '}': t.kind = Tok::RBrace; break;
                case '/': t.kind = Tok::Slash; break;
                case '!':
                    if (match('=')) t.kind = Tok::Ne;
                    else t.kind = Tok::Bang;
                    break;
                case '&':
                    if (!match('&')) failAt("expected '&'", line, col);
                    t.kind = Tok::AndAnd;
                    break;
                case '|':
                    if (!match('|')) failAt("expected '|'", line, col);
                    t.kind = Tok::OrOr;
                    break;
                case '<':
                    if (match('=')) t.kind = Tok::Le;
                    else t.kind = Tok::Lt;
                    break;
                case '>':
                    if (match('=')) t.kind = Tok::Ge;
                    else t.kind = Tok::Gt;
                    break;
                case '=':
                    if (match('=')) t.kind = Tok::Eq;
                    else t.kind = Tok::Assign;
                    break;
                default:
                    failAt(string("unexpected character '") + c + "'", t.line, t.col);
            }
            t.text = string(1, c);
            out.push_back(t);
        }
    }

private:
    string src;
    size_t pos = 0;
    int line = 1;
    int col = 1;

    bool eof() const { return pos >= src.size(); }
    char peek(int n = 0) const {
        if (pos + n >= src.size()) return '\0';
        return src[pos + n];
    }
    char advance() {
        char c = src[pos++];
        if (c == '\n') {
            ++line;
            col = 1;
        } else {
            ++col;
        }
        return c;
    }
    bool match(char c) {
        if (peek() != c) return false;
        advance();
        return true;
    }
    void skipSpaceAndComments() {
        while (!eof()) {
            if (isspace(static_cast<unsigned char>(peek()))) {
                advance();
                continue;
            }
            if (peek() == '/' && peek(1) == '/') {
                while (!eof() && peek() != '\n') advance();
                continue;
            }
            if (peek() == '/' && peek(1) == '*') {
                advance();
                advance();
                while (!eof()) {
                    if (peek() == '*' && peek(1) == '/') {
                        advance();
                        advance();
                        break;
                    }
                    advance();
                }
                continue;
            }
            break;
        }
    }
    string readIdent() {
        string s;
        while (!eof() && (isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
            s.push_back(advance());
        }
        return s;
    }
    string readNumber() {
        string s;
        while (!eof() && isdigit(static_cast<unsigned char>(peek()))) {
            s.push_back(advance());
        }
        return s;
    }
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

struct Expr {
    enum class Kind { Number, Var, Call, Unary, Binary } kind;
    long long value = 0;
    string name;
    string op;
    unique_ptr<Expr> lhs;
    unique_ptr<Expr> rhs;
    vector<unique_ptr<Expr>> args;
};

struct Decl {
    bool isConst = false;
    string name;
    unique_ptr<Expr> init;
};

struct Stmt {
    enum class Kind {
        Block,
        Empty,
        ExprStmt,
        Assign,
        DeclStmt,
        If,
        While,
        Break,
        Continue,
        Return
    } kind;
    vector<unique_ptr<Stmt>> stmts;
    unique_ptr<Decl> decl;
    string name;
    unique_ptr<Expr> expr;
    unique_ptr<Stmt> thenStmt;
    unique_ptr<Stmt> elseStmt;
    unique_ptr<Stmt> body;
};

struct Function {
    bool returnsVoid = false;
    string name;
    vector<string> params;
    unique_ptr<Stmt> body;
};

struct TopItem {
    enum class Kind { Decl, Func } kind;
    unique_ptr<Decl> decl;
    unique_ptr<Function> func;
};

struct Program {
    vector<TopItem> items;
};

class Parser {
public:
    explicit Parser(vector<Token> tokens) : toks(std::move(tokens)) {}

    Program parseProgram() {
        Program p;
        while (!check(Tok::End)) {
            p.items.push_back(parseTopItem());
        }
        return p;
    }

private:
    vector<Token> toks;
    size_t pos = 0;

    const Token &peek(int n = 0) const {
        size_t i = min(pos + static_cast<size_t>(n), toks.size() - 1);
        return toks[i];
    }
    bool check(Tok k) const { return peek().kind == k; }
    bool match(Tok k) {
        if (!check(k)) return false;
        ++pos;
        return true;
    }
    const Token &expect(Tok k, const string &what) {
        if (!check(k)) failAt("expected " + what + ", got '" + peek().text + "'", peek().line, peek().col);
        return toks[pos++];
    }

    TopItem parseTopItem() {
        bool isConst = match(Tok::KwConst);
        if (isConst) {
            expect(Tok::KwInt, "'int'");
            auto d = parseDeclTail(true);
            TopItem item;
            item.kind = TopItem::Kind::Decl;
            item.decl = std::move(d);
            return item;
        }

        bool returnsVoid = false;
        if (match(Tok::KwVoid)) {
            returnsVoid = true;
        } else {
            expect(Tok::KwInt, "'int' or 'void'");
        }
        string name = expect(Tok::Ident, "identifier").text;
        if (match(Tok::LParen)) {
            auto f = make_unique<Function>();
            f->returnsVoid = returnsVoid;
            f->name = name;
            if (!check(Tok::RParen)) {
                do {
                    expect(Tok::KwInt, "'int'");
                    f->params.push_back(expect(Tok::Ident, "parameter name").text);
                } while (match(Tok::Comma));
            }
            expect(Tok::RParen, "')'");
            f->body = parseBlock();
            TopItem item;
            item.kind = TopItem::Kind::Func;
            item.func = std::move(f);
            return item;
        }
        if (returnsVoid) failAt("global variable cannot be void", peek().line, peek().col);
        auto d = make_unique<Decl>();
        d->isConst = false;
        d->name = name;
        expect(Tok::Assign, "'='");
        d->init = parseExpr();
        expect(Tok::Semi, "';'");
        TopItem item;
        item.kind = TopItem::Kind::Decl;
        item.decl = std::move(d);
        return item;
    }

    unique_ptr<Decl> parseDeclTail(bool isConst) {
        auto d = make_unique<Decl>();
        d->isConst = isConst;
        d->name = expect(Tok::Ident, "identifier").text;
        expect(Tok::Assign, "'='");
        d->init = parseExpr();
        expect(Tok::Semi, "';'");
        return d;
    }

    unique_ptr<Stmt> parseBlock() {
        auto s = make_unique<Stmt>();
        s->kind = Stmt::Kind::Block;
        expect(Tok::LBrace, "'{'");
        while (!check(Tok::RBrace)) {
            s->stmts.push_back(parseStmt());
        }
        expect(Tok::RBrace, "'}'");
        return s;
    }

    unique_ptr<Stmt> parseStmt() {
        if (check(Tok::LBrace)) return parseBlock();
        if (match(Tok::Semi)) {
            auto s = make_unique<Stmt>();
            s->kind = Stmt::Kind::Empty;
            return s;
        }
        if (match(Tok::KwConst)) {
            expect(Tok::KwInt, "'int'");
            auto s = make_unique<Stmt>();
            s->kind = Stmt::Kind::DeclStmt;
            s->decl = parseDeclTail(true);
            return s;
        }
        if (match(Tok::KwInt)) {
            auto s = make_unique<Stmt>();
            s->kind = Stmt::Kind::DeclStmt;
            s->decl = parseDeclTail(false);
            return s;
        }
        if (match(Tok::KwIf)) {
            auto s = make_unique<Stmt>();
            s->kind = Stmt::Kind::If;
            expect(Tok::LParen, "'('");
            s->expr = parseExpr();
            expect(Tok::RParen, "')'");
            s->thenStmt = parseStmt();
            if (match(Tok::KwElse)) s->elseStmt = parseStmt();
            return s;
        }
        if (match(Tok::KwWhile)) {
            auto s = make_unique<Stmt>();
            s->kind = Stmt::Kind::While;
            expect(Tok::LParen, "'('");
            s->expr = parseExpr();
            expect(Tok::RParen, "')'");
            s->body = parseStmt();
            return s;
        }
        if (match(Tok::KwBreak)) {
            auto s = make_unique<Stmt>();
            s->kind = Stmt::Kind::Break;
            expect(Tok::Semi, "';'");
            return s;
        }
        if (match(Tok::KwContinue)) {
            auto s = make_unique<Stmt>();
            s->kind = Stmt::Kind::Continue;
            expect(Tok::Semi, "';'");
            return s;
        }
        if (match(Tok::KwReturn)) {
            auto s = make_unique<Stmt>();
            s->kind = Stmt::Kind::Return;
            if (!check(Tok::Semi)) s->expr = parseExpr();
            expect(Tok::Semi, "';'");
            return s;
        }
        if (check(Tok::Ident) && peek(1).kind == Tok::Assign) {
            auto s = make_unique<Stmt>();
            s->kind = Stmt::Kind::Assign;
            s->name = expect(Tok::Ident, "identifier").text;
            expect(Tok::Assign, "'='");
            s->expr = parseExpr();
            expect(Tok::Semi, "';'");
            return s;
        }
        auto s = make_unique<Stmt>();
        s->kind = Stmt::Kind::ExprStmt;
        s->expr = parseExpr();
        expect(Tok::Semi, "';'");
        return s;
    }

    unique_ptr<Expr> parseExpr() { return parseLOr(); }

    unique_ptr<Expr> parseLOr() {
        auto e = parseLAnd();
        while (match(Tok::OrOr)) e = binary("||", std::move(e), parseLAnd());
        return e;
    }

    unique_ptr<Expr> parseLAnd() {
        auto e = parseRel();
        while (match(Tok::AndAnd)) e = binary("&&", std::move(e), parseRel());
        return e;
    }

    unique_ptr<Expr> parseRel() {
        auto e = parseAdd();
        while (true) {
            if (match(Tok::Lt)) e = binary("<", std::move(e), parseAdd());
            else if (match(Tok::Gt)) e = binary(">", std::move(e), parseAdd());
            else if (match(Tok::Le)) e = binary("<=", std::move(e), parseAdd());
            else if (match(Tok::Ge)) e = binary(">=", std::move(e), parseAdd());
            else if (match(Tok::Eq)) e = binary("==", std::move(e), parseAdd());
            else if (match(Tok::Ne)) e = binary("!=", std::move(e), parseAdd());
            else break;
        }
        return e;
    }

    unique_ptr<Expr> parseAdd() {
        auto e = parseMul();
        while (true) {
            if (match(Tok::Plus)) e = binary("+", std::move(e), parseMul());
            else if (match(Tok::Minus)) e = binary("-", std::move(e), parseMul());
            else break;
        }
        return e;
    }

    unique_ptr<Expr> parseMul() {
        auto e = parseUnary();
        while (true) {
            if (match(Tok::Star)) e = binary("*", std::move(e), parseUnary());
            else if (match(Tok::Slash)) e = binary("/", std::move(e), parseUnary());
            else if (match(Tok::Percent)) e = binary("%", std::move(e), parseUnary());
            else break;
        }
        return e;
    }

    unique_ptr<Expr> parseUnary() {
        if (match(Tok::Plus)) return unary("+", parseUnary());
        if (match(Tok::Minus)) return unary("-", parseUnary());
        if (match(Tok::Bang)) return unary("!", parseUnary());
        return parsePrimary();
    }

    unique_ptr<Expr> parsePrimary() {
        if (match(Tok::Number)) {
            auto e = make_unique<Expr>();
            e->kind = Expr::Kind::Number;
            e->value = toks[pos - 1].value;
            return e;
        }
        if (match(Tok::Ident)) {
            string name = toks[pos - 1].text;
            if (match(Tok::LParen)) {
                auto e = make_unique<Expr>();
                e->kind = Expr::Kind::Call;
                e->name = name;
                if (!check(Tok::RParen)) {
                    do {
                        e->args.push_back(parseExpr());
                    } while (match(Tok::Comma));
                }
                expect(Tok::RParen, "')'");
                return e;
            }
            auto e = make_unique<Expr>();
            e->kind = Expr::Kind::Var;
            e->name = name;
            return e;
        }
        if (match(Tok::LParen)) {
            auto e = parseExpr();
            expect(Tok::RParen, "')'");
            return e;
        }
        failAt("expected expression", peek().line, peek().col);
    }

    static unique_ptr<Expr> unary(string op, unique_ptr<Expr> sub) {
        auto e = make_unique<Expr>();
        e->kind = Expr::Kind::Unary;
        e->op = std::move(op);
        e->lhs = std::move(sub);
        return e;
    }

    static unique_ptr<Expr> binary(string op, unique_ptr<Expr> lhs, unique_ptr<Expr> rhs) {
        auto e = make_unique<Expr>();
        e->kind = Expr::Kind::Binary;
        e->op = std::move(op);
        e->lhs = std::move(lhs);
        e->rhs = std::move(rhs);
        return e;
    }
};

static unique_ptr<Expr> makeNumberExpr(long long value) {
    auto e = make_unique<Expr>();
    e->kind = Expr::Kind::Number;
    e->value = value;
    return e;
}

static bool exprHasCall(const Expr *e) {
    if (!e) return false;
    if (e->kind == Expr::Kind::Call) return true;
    if (exprHasCall(e->lhs.get()) || exprHasCall(e->rhs.get())) return true;
    for (auto &arg : e->args) {
        if (exprHasCall(arg.get())) return true;
    }
    return false;
}

static optional<long long> foldConstExpr(const Expr *e) {
    if (!e) return nullopt;
    switch (e->kind) {
        case Expr::Kind::Number:
            return e->value;
        case Expr::Kind::Var:
        case Expr::Kind::Call:
            return nullopt;
        case Expr::Kind::Unary: {
            auto v = foldConstExpr(e->lhs.get());
            if (!v) return nullopt;
            if (e->op == "+") return *v;
            if (e->op == "-") return -*v;
            if (e->op == "!") return *v == 0;
            return nullopt;
        }
        case Expr::Kind::Binary: {
            auto l = foldConstExpr(e->lhs.get());
            if ((e->op == "&&" || e->op == "||") && l) {
                if (e->op == "&&" && !*l) return 0;
                if (e->op == "||" && *l) return 1;
            }
            auto r = foldConstExpr(e->rhs.get());
            if (!l || !r) return nullopt;
            if (e->op == "+") return *l + *r;
            if (e->op == "-") return *l - *r;
            if (e->op == "*") return *l * *r;
            if (e->op == "/") return *r == 0 ? nullopt : optional<long long>(*l / *r);
            if (e->op == "%") return *r == 0 ? nullopt : optional<long long>(*l % *r);
            if (e->op == "<") return *l < *r;
            if (e->op == ">") return *l > *r;
            if (e->op == "<=") return *l <= *r;
            if (e->op == ">=") return *l >= *r;
            if (e->op == "==") return *l == *r;
            if (e->op == "!=") return *l != *r;
            if (e->op == "&&") return (*l != 0) && (*r != 0);
            if (e->op == "||") return (*l != 0) || (*r != 0);
            return nullopt;
        }
    }
    return nullopt;
}

class Resolver {
public:
    explicit Resolver(Program &program) : prog(program) {}

    void run() {
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Func) resolveFunction(*item.func);
            else if (item.decl) resolveExpr(item.decl->init);
        }
    }

private:
    Program &prog;
    vector<unordered_map<string, string>> scopes;
    int id = 0;

    optional<string> lookup(const string &name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        return nullopt;
    }

    string fresh(const string &name) {
        return name + "#v" + to_string(id++);
    }

    void enter() { scopes.push_back({}); }
    void leave() { scopes.pop_back(); }

    void bind(string &name) {
        string old = name;
        string renamed = fresh(old);
        scopes.back()[old] = renamed;
        name = renamed;
    }

    void resolveFunction(Function &f) {
        enter();
        for (auto &param : f.params) bind(param);
        resolveStmt(f.body);
        leave();
    }

    void resolveStmt(unique_ptr<Stmt> &s) {
        if (!s) return;
        switch (s->kind) {
            case Stmt::Kind::Block:
                enter();
                for (auto &child : s->stmts) resolveStmt(child);
                leave();
                break;
            case Stmt::Kind::DeclStmt:
                resolveExpr(s->decl->init);
                bind(s->decl->name);
                break;
            case Stmt::Kind::Assign:
                if (auto renamed = lookup(s->name)) s->name = *renamed;
                resolveExpr(s->expr);
                break;
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Return:
                resolveExpr(s->expr);
                break;
            case Stmt::Kind::If:
                resolveExpr(s->expr);
                resolveStmt(s->thenStmt);
                resolveStmt(s->elseStmt);
                break;
            case Stmt::Kind::While:
                resolveExpr(s->expr);
                resolveStmt(s->body);
                break;
            default:
                break;
        }
    }

    void resolveExpr(unique_ptr<Expr> &e) {
        if (!e) return;
        if (e->kind == Expr::Kind::Var) {
            if (auto renamed = lookup(e->name)) e->name = *renamed;
            return;
        }
        resolveExpr(e->lhs);
        resolveExpr(e->rhs);
        for (auto &arg : e->args) resolveExpr(arg);
    }
};

struct OptValue {
    enum class Kind { Unknown, Const, Copy } kind = Kind::Unknown;
    long long value = 0;
    string copy;
};

class Optimizer {
public:
    explicit Optimizer(Program &program) : prog(program) {
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Decl) globals.insert(item.decl->name);
        }
    }

    void run() {
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Decl) {
                optExpr(item.decl->init);
            } else {
                env.clear();
                optStmt(item.func->body);
            }
        }
    }

private:
    Program &prog;
    unordered_set<string> globals;
    vector<unordered_map<string, OptValue>> env;
    int loopDepth = 0;
    int keepEffectDepth = 0;

    bool isGlobal(const string &name) const { return globals.count(name) != 0; }
    void enter() { env.push_back({}); }
    void leave() { env.pop_back(); }

    optional<OptValue> lookupValue(const string &name) const {
        if (isGlobal(name)) return nullopt;
        for (auto it = env.rbegin(); it != env.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        return nullopt;
    }

    string resolveCopy(string name) const {
        unordered_set<string> seen;
        while (!seen.count(name)) {
            seen.insert(name);
            auto v = lookupValue(name);
            if (!v || v->kind != OptValue::Kind::Copy) break;
            name = v->copy;
        }
        return name;
    }

    void eraseValue(const string &name) {
        for (auto &scope : env) scope.erase(name);
        for (auto &scope : env) {
            for (auto it = scope.begin(); it != scope.end();) {
                if (it->second.kind == OptValue::Kind::Copy && resolveCopy(it->second.copy) == name) {
                    it = scope.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void setValue(const string &name, OptValue value) {
        if (isGlobal(name) || env.empty()) return;
        eraseValue(name);
        for (auto it = env.rbegin(); it != env.rend(); ++it) {
            if (it->count(name)) {
                (*it)[name] = std::move(value);
                return;
            }
        }
        env.back()[name] = std::move(value);
    }

    OptValue valueFromExpr(const Expr *e) const {
        if (auto v = foldConstExpr(e)) return OptValue{OptValue::Kind::Const, *v, ""};
        if (e && e->kind == Expr::Kind::Var && !isGlobal(e->name)) {
            return OptValue{OptValue::Kind::Copy, 0, resolveCopy(e->name)};
        }
        return {};
    }

    void optExpr(unique_ptr<Expr> &e) {
        if (!e) return;
        switch (e->kind) {
            case Expr::Kind::Number:
                return;
            case Expr::Kind::Var: {
                if (auto v = lookupValue(e->name)) {
                    if (v->kind == OptValue::Kind::Const) {
                        e = makeNumberExpr(v->value);
                    } else if (v->kind == OptValue::Kind::Copy) {
                        e->name = resolveCopy(v->copy);
                    }
                }
                return;
            }
            case Expr::Kind::Call:
                for (auto &arg : e->args) optExpr(arg);
                return;
            case Expr::Kind::Unary:
                optExpr(e->lhs);
                if (auto v = foldConstExpr(e.get())) e = makeNumberExpr(*v);
                return;
            case Expr::Kind::Binary:
                optExpr(e->lhs);
                optExpr(e->rhs);
                simplifyBinary(e);
                return;
        }
    }

    void simplifyBinary(unique_ptr<Expr> &e) {
        if (auto v = foldConstExpr(e.get())) {
            e = makeNumberExpr(*v);
            return;
        }
        if (!e || e->kind != Expr::Kind::Binary) return;
        auto l = foldConstExpr(e->lhs.get());
        auto r = foldConstExpr(e->rhs.get());
        if (e->op == "+" && r && *r == 0) e = std::move(e->lhs);
        else if (e->op == "+" && l && *l == 0) e = std::move(e->rhs);
        else if (e->op == "-" && r && *r == 0) e = std::move(e->lhs);
        else if (e->op == "*" && r && *r == 1) e = std::move(e->lhs);
        else if (e->op == "*" && l && *l == 1) e = std::move(e->rhs);
        else if (e->op == "*" && ((r && *r == 0) || (l && *l == 0))) e = makeNumberExpr(0);
        else if (e->op == "/" && r && *r == 1) e = std::move(e->lhs);
        else if (e->op == "%" && r && *r == 1) e = makeNumberExpr(0);
        else if (e->op == "&&" && l && *l == 0) e = makeNumberExpr(0);
        else if (e->op == "||" && l && *l != 0) e = makeNumberExpr(1);
    }

    bool alwaysJumps(const Stmt *s) const {
        if (!s) return false;
        switch (s->kind) {
            case Stmt::Kind::Return:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return true;
            case Stmt::Kind::Block:
                return !s->stmts.empty() && alwaysJumps(s->stmts.back().get());
            case Stmt::Kind::If:
                return s->elseStmt && alwaysJumps(s->thenStmt.get()) && alwaysJumps(s->elseStmt.get());
            default:
                return false;
        }
    }

    unique_ptr<Stmt> emptyStmt() const {
        auto s = make_unique<Stmt>();
        s->kind = Stmt::Kind::Empty;
        return s;
    }

    void optStmt(unique_ptr<Stmt> &s) {
        if (!s) return;
        switch (s->kind) {
            case Stmt::Kind::Block:
                enter();
                for (auto &child : s->stmts) {
                    optStmt(child);
                    if (alwaysJumps(child.get())) break;
                }
                dceBlock(s->stmts);
                leave();
                break;
            case Stmt::Kind::DeclStmt:
                optExpr(s->decl->init);
                setValue(s->decl->name, valueFromExpr(s->decl->init.get()));
                break;
            case Stmt::Kind::Assign:
                optExpr(s->expr);
                setValue(s->name, valueFromExpr(s->expr.get()));
                break;
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Return:
                optExpr(s->expr);
                break;
            case Stmt::Kind::If: {
                optExpr(s->expr);
                if (auto v = foldConstExpr(s->expr.get())) {
                    if (*v) {
                        s = std::move(s->thenStmt);
                        ++keepEffectDepth;
                        optStmt(s);
                        --keepEffectDepth;
                    } else if (s->elseStmt) {
                        s = std::move(s->elseStmt);
                        ++keepEffectDepth;
                        optStmt(s);
                        --keepEffectDepth;
                    } else {
                        s = emptyStmt();
                    }
                    break;
                }
                auto saved = env;
                optStmt(s->thenStmt);
                auto thenAssigned = assignedInStmt(s->thenStmt.get());
                env = saved;
                optStmt(s->elseStmt);
                auto elseAssigned = assignedInStmt(s->elseStmt.get());
                env = saved;
                for (auto &name : thenAssigned) eraseValue(name);
                for (auto &name : elseAssigned) eraseValue(name);
                break;
            }
            case Stmt::Kind::While: {
                auto saved = env;
                auto assigned = assignedInStmt(s->body.get());
                for (auto &name : assigned) eraseValue(name);
                optExpr(s->expr);
                if (auto v = foldConstExpr(s->expr.get()); v && !*v) {
                    s = emptyStmt();
                    break;
                }
                ++loopDepth;
                optStmt(s->body);
                --loopDepth;
                env = saved;
                for (auto &name : assigned) eraseValue(name);
                break;
            }
            default:
                break;
        }
    }

    unordered_set<string> varsInExpr(const Expr *e) const {
        unordered_set<string> out;
        collectVars(e, out);
        return out;
    }

    void collectVars(const Expr *e, unordered_set<string> &out) const {
        if (!e) return;
        if (e->kind == Expr::Kind::Var) {
            out.insert(e->name);
            return;
        }
        collectVars(e->lhs.get(), out);
        collectVars(e->rhs.get(), out);
        for (auto &arg : e->args) collectVars(arg.get(), out);
    }

    unordered_set<string> assignedInStmt(const Stmt *s) const {
        unordered_set<string> out;
        collectAssigned(s, out);
        return out;
    }

    void collectAssigned(const Stmt *s, unordered_set<string> &out) const {
        if (!s) return;
        if (s->kind == Stmt::Kind::Assign) out.insert(s->name);
        if (s->kind == Stmt::Kind::DeclStmt) out.insert(s->decl->name);
        for (auto &child : s->stmts) collectAssigned(child.get(), out);
        collectAssigned(s->thenStmt.get(), out);
        collectAssigned(s->elseStmt.get(), out);
        collectAssigned(s->body.get(), out);
    }

    unordered_set<string> usedInStmt(const Stmt *s) const {
        unordered_set<string> out;
        collectUsed(s, out);
        return out;
    }

    void collectUsed(const Stmt *s, unordered_set<string> &out) const {
        if (!s) return;
        auto add = [&](const Expr *e) {
            auto vars = varsInExpr(e);
            out.insert(vars.begin(), vars.end());
        };
        if (s->kind == Stmt::Kind::DeclStmt) add(s->decl->init.get());
        if (s->kind == Stmt::Kind::Assign || s->kind == Stmt::Kind::ExprStmt || s->kind == Stmt::Kind::If ||
            s->kind == Stmt::Kind::While || s->kind == Stmt::Kind::Return) {
            add(s->expr.get());
        }
        for (auto &child : s->stmts) collectUsed(child.get(), out);
        collectUsed(s->thenStmt.get(), out);
        collectUsed(s->elseStmt.get(), out);
        collectUsed(s->body.get(), out);
    }

    void dceBlock(vector<unique_ptr<Stmt>> &stmts) {
        vector<unique_ptr<Stmt>> reachable;
        for (auto &s : stmts) {
            reachable.push_back(std::move(s));
            if (alwaysJumps(reachable.back().get())) break;
        }
        stmts = std::move(reachable);

        unordered_set<string> live;
        vector<unique_ptr<Stmt>> kept;
        for (int i = static_cast<int>(stmts.size()) - 1; i >= 0; --i) {
            auto &s = stmts[i];
            bool keep = true;
            switch (s->kind) {
                case Stmt::Kind::Empty:
                    keep = false;
                    break;
                case Stmt::Kind::ExprStmt:
                    keep = exprHasCall(s->expr.get());
                    if (keep) addLive(live, varsInExpr(s->expr.get()));
                    break;
                case Stmt::Kind::DeclStmt: {
                    bool needed = live.count(s->decl->name) || exprHasCall(s->decl->init.get());
                    if (loopDepth > 0 || keepEffectDepth > 0) needed = true;
                    keep = needed;
                    if (keep) {
                        live.erase(s->decl->name);
                        addLive(live, varsInExpr(s->decl->init.get()));
                    }
                    break;
                }
                case Stmt::Kind::Assign: {
                    bool globalWrite = isGlobal(s->name);
                    bool needed = globalWrite || live.count(s->name) || exprHasCall(s->expr.get());
                    if (loopDepth > 0 || keepEffectDepth > 0) needed = true;
                    keep = needed;
                    if (keep) {
                        live.erase(s->name);
                        addLive(live, varsInExpr(s->expr.get()));
                    }
                    break;
                }
                case Stmt::Kind::Return:
                    live.clear();
                    addLive(live, varsInExpr(s->expr.get()));
                    break;
                case Stmt::Kind::If:
                case Stmt::Kind::While:
                    addLive(live, varsInExpr(s->expr.get()));
                    addLive(live, usedInStmt(s.get()));
                    break;
                default:
                    break;
            }
            if (keep) kept.push_back(std::move(s));
        }
        reverse(kept.begin(), kept.end());
        stmts = std::move(kept);
    }

    static void addLive(unordered_set<string> &live, const unordered_set<string> &vars) {
        live.insert(vars.begin(), vars.end());
    }
};

struct Symbol {
    bool isConst = false;
    long long constValue = 0;
    bool isGlobal = false;
    string label;
    int offset = 0;
    string reg;
};

struct FuncInfo {
    bool returnsVoid = false;
    int params = 0;
};

class CodeGen {
public:
    explicit CodeGen(Program &program) : prog(program) {}

    string generate() {
        collectFunctions();
        processGlobals();
        out << ".text\n";
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Func) genFunction(*item.func);
        }
        return out.str();
    }

private:
    Program &prog;
    ostringstream out;
    unordered_map<string, Symbol> globals;
    unordered_map<string, FuncInfo> funcs;
    vector<unordered_map<string, Symbol>> scopes;
    vector<string> breakLabels;
    vector<string> continueLabels;
    string returnLabel;
    string currentFunctionName;
    string functionBodyLabel;
    vector<string> currentParams;
    vector<string> savedVarRegs;
    int labelId = 0;
    int nextSlot = 0;
    int nextVarReg = 0;
    int localBaseOffset = -12;
    int frameSize = 0;

    static int alignTo(int x, int a) { return (x + a - 1) / a * a; }
    static bool fits12(long long x) { return x >= -2048 && x <= 2047; }
    static string globalLabel(const string &name) { return ".Lglob_" + name; }

    string newLabel(const string &prefix) {
        return ".L" + prefix + "_" + to_string(labelId++);
    }

    void emit(const string &s) { out << "    " << s << "\n"; }
    void emitLabel(const string &s) { out << s << ":\n"; }

    void adjustSp(int bytes) {
        if (bytes == 0) return;
        if (fits12(bytes)) {
            emit("addi sp, sp, " + to_string(bytes));
        } else {
            emit("li t6, " + to_string(bytes));
            emit("add sp, sp, t6");
        }
    }

    void loadMem(const string &reg, const string &base, int offset) {
        if (fits12(offset)) {
            emit("lw " + reg + ", " + to_string(offset) + "(" + base + ")");
        } else {
            emit("li t6, " + to_string(offset));
            emit("add t6, " + base + ", t6");
            emit("lw " + reg + ", 0(t6)");
        }
    }

    void storeMem(const string &reg, const string &base, int offset) {
        if (fits12(offset)) {
            emit("sw " + reg + ", " + to_string(offset) + "(" + base + ")");
        } else {
            emit("li t6, " + to_string(offset));
            emit("add t6, " + base + ", t6");
            emit("sw " + reg + ", 0(t6)");
        }
    }

    void pushA0() {
        pushReg("a0");
    }

    void pushReg(const string &reg) {
        adjustSp(-16);
        storeMem(reg, "sp", 12);
    }

    void popTo(const string &reg) {
        loadMem(reg, "sp", 12);
        adjustSp(16);
    }

    void collectFunctions() {
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Func) {
                funcs[item.func->name] = FuncInfo{item.func->returnsVoid, static_cast<int>(item.func->params.size())};
            }
        }
    }

    void processGlobals() {
        vector<pair<string, long long>> data;
        for (auto &item : prog.items) {
            if (item.kind != TopItem::Kind::Decl) continue;
            Decl &d = *item.decl;
            if (d.isConst) {
                long long v = evalConst(d.init.get());
                globals[d.name] = Symbol{true, v, true, "", 0, ""};
            } else {
                long long v = evalConst(d.init.get());
                string label = globalLabel(d.name);
                globals[d.name] = Symbol{false, 0, true, label, 0, ""};
                data.push_back({label, v});
            }
        }
        if (!data.empty()) {
            out << ".data\n";
            for (auto &[label, value] : data) {
                out << ".align 2\n";
                out << label << ":\n";
                out << "    .word " << value << "\n";
            }
        }
    }

    optional<Symbol> lookup(const string &name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        auto g = globals.find(name);
        if (g != globals.end()) return g->second;
        return nullopt;
    }

    long long evalConst(const Expr *e) {
        switch (e->kind) {
            case Expr::Kind::Number:
                return e->value;
            case Expr::Kind::Var: {
                auto sym = lookup(e->name);
                if (!sym || !sym->isConst) return 0;
                return sym->constValue;
            }
            case Expr::Kind::Unary: {
                long long v = evalConst(e->lhs.get());
                if (e->op == "+") return v;
                if (e->op == "-") return -v;
                if (e->op == "!") return v == 0;
                return 0;
            }
            case Expr::Kind::Binary: {
                if (e->op == "&&") {
                    long long l = evalConst(e->lhs.get());
                    if (!l) return 0;
                    return evalConst(e->rhs.get()) != 0;
                }
                if (e->op == "||") {
                    long long l = evalConst(e->lhs.get());
                    if (l) return 1;
                    return evalConst(e->rhs.get()) != 0;
                }
                long long l = evalConst(e->lhs.get());
                long long r = evalConst(e->rhs.get());
                if (e->op == "+") return l + r;
                if (e->op == "-") return l - r;
                if (e->op == "*") return l * r;
                if (e->op == "/") return r == 0 ? 0 : l / r;
                if (e->op == "%") return r == 0 ? 0 : l % r;
                if (e->op == "<") return l < r;
                if (e->op == ">") return l > r;
                if (e->op == "<=") return l <= r;
                if (e->op == ">=") return l >= r;
                if (e->op == "==") return l == r;
                if (e->op == "!=") return l != r;
                return 0;
            }
            case Expr::Kind::Call:
                return 0;
        }
        return 0;
    }

    optional<long long> tryConst(const Expr *e) {
        switch (e->kind) {
            case Expr::Kind::Number:
                return e->value;
            case Expr::Kind::Var: {
                auto sym = lookup(e->name);
                if (sym && sym->isConst) return sym->constValue;
                return nullopt;
            }
            case Expr::Kind::Unary: {
                auto v = tryConst(e->lhs.get());
                if (!v) return nullopt;
                if (e->op == "+") return *v;
                if (e->op == "-") return -*v;
                if (e->op == "!") return *v == 0;
                return nullopt;
            }
            case Expr::Kind::Binary: {
                if (e->op == "&&") {
                    auto l = tryConst(e->lhs.get());
                    if (!l) return nullopt;
                    if (!*l) return 0;
                    auto r = tryConst(e->rhs.get());
                    if (!r) return nullopt;
                    return *r != 0;
                }
                if (e->op == "||") {
                    auto l = tryConst(e->lhs.get());
                    if (!l) return nullopt;
                    if (*l) return 1;
                    auto r = tryConst(e->rhs.get());
                    if (!r) return nullopt;
                    return *r != 0;
                }
                auto l = tryConst(e->lhs.get());
                auto r = tryConst(e->rhs.get());
                if (!l || !r) return nullopt;
                if (e->op == "+") return *l + *r;
                if (e->op == "-") return *l - *r;
                if (e->op == "*") return *l * *r;
                if (e->op == "/") {
                    if (*r == 0) return nullopt;
                    return *l / *r;
                }
                if (e->op == "%") {
                    if (*r == 0) return nullopt;
                    return *l % *r;
                }
                if (e->op == "<") return *l < *r;
                if (e->op == ">") return *l > *r;
                if (e->op == "<=") return *l <= *r;
                if (e->op == ">=") return *l >= *r;
                if (e->op == "==") return *l == *r;
                if (e->op == "!=") return *l != *r;
                return nullopt;
            }
            case Expr::Kind::Call:
                return nullopt;
        }
        return nullopt;
    }

    bool hasCall(const Expr *e) const {
        if (!e) return false;
        if (e->kind == Expr::Kind::Call) return true;
        if (hasCall(e->lhs.get()) || hasCall(e->rhs.get())) return true;
        for (auto &arg : e->args) {
            if (hasCall(arg.get())) return true;
        }
        return false;
    }

    bool stmtAlwaysJumps(const Stmt *s) const {
        if (!s) return false;
        switch (s->kind) {
            case Stmt::Kind::Return:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return true;
            case Stmt::Kind::Block:
                return !s->stmts.empty() && stmtAlwaysJumps(s->stmts.back().get());
            case Stmt::Kind::If:
                return s->elseStmt && stmtAlwaysJumps(s->thenStmt.get()) && stmtAlwaysJumps(s->elseStmt.get());
            default:
                return false;
        }
    }

    int countSlots(const Function &f) {
        int n = static_cast<int>(f.params.size());
        n += countSlots(f.body.get());
        return n;
    }

    int countSlots(const Stmt *s) {
        if (!s) return 0;
        int n = 0;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) n += countSlots(child.get());
                break;
            case Stmt::Kind::DeclStmt:
                if (!s->decl->isConst) n += 1;
                break;
            case Stmt::Kind::If:
                n += countSlots(s->thenStmt.get());
                n += countSlots(s->elseStmt.get());
                break;
            case Stmt::Kind::While:
                n += countSlots(s->body.get());
                break;
            default:
                break;
        }
        return n;
    }

    int allocSlot() {
        int off = localBaseOffset - nextSlot * 4;
        ++nextSlot;
        return off;
    }

    string allocVarReg() {
        if (nextVarReg >= static_cast<int>(savedVarRegs.size())) return "";
        return savedVarRegs[nextVarReg++];
    }

    void enterScope() { scopes.push_back({}); }
    void leaveScope() { scopes.pop_back(); }

    void genFunction(Function &f) {
        int slots = countSlots(f);
        static const vector<string> allVarRegs = {"s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11"};
        savedVarRegs.assign(allVarRegs.begin(), allVarRegs.begin() + min<int>(allVarRegs.size(), slots));
        frameSize = alignTo(8 + static_cast<int>(savedVarRegs.size()) * 4 + slots * 4, 16);
        nextSlot = 0;
        nextVarReg = 0;
        localBaseOffset = -12 - static_cast<int>(savedVarRegs.size()) * 4;
        scopes.clear();
        breakLabels.clear();
        continueLabels.clear();
        currentFunctionName = f.name;
        currentParams = f.params;
        returnLabel = newLabel("return_" + f.name);
        functionBodyLabel = newLabel("body_" + f.name);

        out << ".globl " << f.name << "\n";
        emitLabel(f.name);
        adjustSp(-frameSize);
        storeMem("ra", "sp", frameSize - 4);
        storeMem("s0", "sp", frameSize - 8);
        for (int i = 0; i < static_cast<int>(savedVarRegs.size()); ++i) {
            storeMem(savedVarRegs[i], "sp", frameSize - 12 - i * 4);
        }
        if (fits12(frameSize)) emit("addi s0, sp, " + to_string(frameSize));
        else {
            emit("li t6, " + to_string(frameSize));
            emit("add s0, sp, t6");
        }

        enterScope();
        for (int i = 0; i < static_cast<int>(f.params.size()); ++i) {
            string reg = allocVarReg();
            int off = reg.empty() ? allocSlot() : 0;
            scopes.back()[f.params[i]] = Symbol{false, 0, false, "", off, reg};
            if (i < 8) {
                if (!reg.empty()) emit("mv " + reg + ", a" + to_string(i));
                else storeMem("a" + to_string(i), "s0", off);
            } else {
                loadMem("t0", "s0", (i - 8) * 4);
                if (!reg.empty()) emit("mv " + reg + ", t0");
                else storeMem("t0", "s0", off);
            }
        }

        emitLabel(functionBodyLabel);
        genStmt(f.body.get());
        emit("li a0, 0");
        emitLabel(returnLabel);
        for (int i = 0; i < static_cast<int>(savedVarRegs.size()); ++i) {
            loadMem(savedVarRegs[i], "s0", -12 - i * 4);
        }
        loadMem("ra", "s0", -4);
        loadMem("s0", "s0", -8);
        adjustSp(frameSize);
        emit("ret");
        leaveScope();
        out << "\n";
    }

    void genStmt(const Stmt *s) {
        switch (s->kind) {
            case Stmt::Kind::Block:
                enterScope();
                for (auto &child : s->stmts) {
                    genStmt(child.get());
                    if (stmtAlwaysJumps(child.get())) break;
                }
                leaveScope();
                break;
            case Stmt::Kind::Empty:
                break;
            case Stmt::Kind::ExprStmt:
                genExpr(s->expr.get());
                break;
            case Stmt::Kind::Assign:
                genExpr(s->expr.get());
                storeVar(s->name);
                break;
            case Stmt::Kind::DeclStmt:
                genDecl(*s->decl);
                break;
            case Stmt::Kind::If:
                genIf(s);
                break;
            case Stmt::Kind::While:
                genWhile(s);
                break;
            case Stmt::Kind::Break:
                if (!breakLabels.empty()) emit("j " + breakLabels.back());
                break;
            case Stmt::Kind::Continue:
                if (!continueLabels.empty()) emit("j " + continueLabels.back());
                break;
            case Stmt::Kind::Return:
                if (s->expr && s->expr->kind == Expr::Kind::Call && s->expr->name == currentFunctionName) {
                    genTailSelfCall(s->expr.get());
                } else {
                    if (s->expr) genExpr(s->expr.get());
                    else emit("li a0, 0");
                    emit("j " + returnLabel);
                }
                break;
        }
    }

    void genDecl(const Decl &d) {
        if (d.isConst) {
            long long v = evalConst(d.init.get());
            scopes.back()[d.name] = Symbol{true, v, false, "", 0, ""};
            return;
        }
        genExpr(d.init.get());
        string reg = allocVarReg();
        int off = reg.empty() ? allocSlot() : 0;
        scopes.back()[d.name] = Symbol{false, 0, false, "", off, reg};
        if (!reg.empty()) emit("mv " + reg + ", a0");
        else storeMem("a0", "s0", off);
    }

    void genIf(const Stmt *s) {
        if (auto v = tryConst(s->expr.get())) {
            if (*v) genStmt(s->thenStmt.get());
            else if (s->elseStmt) genStmt(s->elseStmt.get());
            return;
        }
        string elseLabel = newLabel("else");
        string endLabel = newLabel("endif");
        genCondFalse(s->expr.get(), elseLabel);
        genStmt(s->thenStmt.get());
        emit("j " + endLabel);
        emitLabel(elseLabel);
        if (s->elseStmt) genStmt(s->elseStmt.get());
        emitLabel(endLabel);
    }

    void genWhile(const Stmt *s) {
        if (auto v = tryConst(s->expr.get()); v && !*v) return;
        string beginLabel = newLabel("while_begin");
        string endLabel = newLabel("while_end");
        emitLabel(beginLabel);
        genCondFalse(s->expr.get(), endLabel);
        breakLabels.push_back(endLabel);
        continueLabels.push_back(beginLabel);
        genStmt(s->body.get());
        continueLabels.pop_back();
        breakLabels.pop_back();
        emit("j " + beginLabel);
        emitLabel(endLabel);
    }

    void genExpr(const Expr *e) {
        if (auto v = tryConst(e)) {
            emit("li a0, " + to_string(*v));
            return;
        }
        if (!hasCall(e)) {
            genExprNoCall(e, "a0", {"t0", "t1", "t2", "t3", "t4", "t5"});
            return;
        }
        switch (e->kind) {
            case Expr::Kind::Number:
                emit("li a0, " + to_string(e->value));
                break;
            case Expr::Kind::Var:
                loadVar(e->name);
                break;
            case Expr::Kind::Unary:
                genUnary(e);
                break;
            case Expr::Kind::Binary:
                genBinary(e);
                break;
            case Expr::Kind::Call:
                genCall(e);
                break;
        }
    }

    void genUnary(const Expr *e) {
        genExpr(e->lhs.get());
        if (e->op == "-") emit("sub a0, x0, a0");
        else if (e->op == "!") emit("sltiu a0, a0, 1");
    }

    void loadVarTo(const string &name, const string &dst) {
        auto sym = lookup(name);
        if (!sym) {
            emit("li " + dst + ", 0");
            return;
        }
        if (sym->isConst) {
            emit("li " + dst + ", " + to_string(sym->constValue));
        } else if (!sym->reg.empty()) {
            if (dst != sym->reg) emit("mv " + dst + ", " + sym->reg);
        } else if (sym->isGlobal) {
            emit("la t6, " + sym->label);
            loadMem(dst, "t6", 0);
        } else {
            loadMem(dst, "s0", sym->offset);
        }
    }

    void storeVarFrom(const string &name, const string &src) {
        auto sym = lookup(name);
        if (!sym || sym->isConst) return;
        if (!sym->reg.empty()) {
            if (src != sym->reg) emit("mv " + sym->reg + ", " + src);
        } else if (sym->isGlobal) {
            emit("la t6, " + sym->label);
            storeMem(src, "t6", 0);
        } else {
            storeMem(src, "s0", sym->offset);
        }
    }

    void storeParamFrom(const string &name, const string &src) {
        if (scopes.empty()) return;
        auto it = scopes.front().find(name);
        if (it == scopes.front().end()) return;
        const Symbol &sym = it->second;
        if (sym.isConst || sym.isGlobal) return;
        if (!sym.reg.empty()) {
            if (src != sym.reg) emit("mv " + sym.reg + ", " + src);
        } else {
            storeMem(src, "s0", sym.offset);
        }
    }

    static bool isPowerOfTwo(long long v) {
        return v > 0 && (v & (v - 1)) == 0;
    }

    static int log2Int(long long v) {
        int n = 0;
        while (v > 1) {
            v >>= 1;
            ++n;
        }
        return n;
    }

    void emitBinaryReg(const string &op, const string &dst, const string &lhs, const string &rhs) {
        if (op == "+") emit("add " + dst + ", " + lhs + ", " + rhs);
        else if (op == "-") emit("sub " + dst + ", " + lhs + ", " + rhs);
        else if (op == "*") emit("mul " + dst + ", " + lhs + ", " + rhs);
        else if (op == "/") emit("div " + dst + ", " + lhs + ", " + rhs);
        else if (op == "%") emit("rem " + dst + ", " + lhs + ", " + rhs);
        else if (op == "<") emit("slt " + dst + ", " + lhs + ", " + rhs);
        else if (op == ">") emit("slt " + dst + ", " + rhs + ", " + lhs);
        else if (op == "<=") {
            emit("slt " + dst + ", " + rhs + ", " + lhs);
            emit("xori " + dst + ", " + dst + ", 1");
        } else if (op == ">=") {
            emit("slt " + dst + ", " + lhs + ", " + rhs);
            emit("xori " + dst + ", " + dst + ", 1");
        } else if (op == "==") {
            emit("sub " + dst + ", " + lhs + ", " + rhs);
            emit("sltiu " + dst + ", " + dst + ", 1");
        } else if (op == "!=") {
            emit("sub " + dst + ", " + lhs + ", " + rhs);
            emit("sltu " + dst + ", x0, " + dst);
        }
    }

    void genExprNoCall(const Expr *e, const string &dst, vector<string> regs) {
        if (auto v = tryConst(e)) {
            emit("li " + dst + ", " + to_string(*v));
            return;
        }
        switch (e->kind) {
            case Expr::Kind::Number:
                emit("li " + dst + ", " + to_string(e->value));
                return;
            case Expr::Kind::Var:
                loadVarTo(e->name, dst);
                return;
            case Expr::Kind::Call:
                genExpr(e);
                if (dst != "a0") emit("mv " + dst + ", a0");
                return;
            case Expr::Kind::Unary:
                genExprNoCall(e->lhs.get(), dst, regs);
                if (e->op == "-") emit("sub " + dst + ", x0, " + dst);
                else if (e->op == "!") emit("sltiu " + dst + ", " + dst + ", 1");
                return;
            case Expr::Kind::Binary:
                break;
        }

        if (e->op == "&&") {
            string falseLabel = newLabel("land_false");
            string endLabel = newLabel("land_end");
            genCondFalse(e->lhs.get(), falseLabel);
            genCondFalse(e->rhs.get(), falseLabel);
            emit("li " + dst + ", 1");
            emit("j " + endLabel);
            emitLabel(falseLabel);
            emit("li " + dst + ", 0");
            emitLabel(endLabel);
            return;
        }
        if (e->op == "||") {
            string trueLabel = newLabel("lor_true");
            string endLabel = newLabel("lor_end");
            genCondTrue(e->lhs.get(), trueLabel);
            genCondTrue(e->rhs.get(), trueLabel);
            emit("li " + dst + ", 0");
            emit("j " + endLabel);
            emitLabel(trueLabel);
            emit("li " + dst + ", 1");
            emitLabel(endLabel);
            return;
        }

        if (auto rhs = tryConst(e->rhs.get())) {
            if (e->op == "+" && *rhs == 0) {
                genExprNoCall(e->lhs.get(), dst, regs);
                return;
            }
            if (e->op == "-" && *rhs == 0) {
                genExprNoCall(e->lhs.get(), dst, regs);
                return;
            }
            if ((e->op == "+" || e->op == "-") && fits12(e->op == "+" ? *rhs : -*rhs)) {
                genExprNoCall(e->lhs.get(), dst, regs);
                emit("addi " + dst + ", " + dst + ", " + to_string(e->op == "+" ? *rhs : -*rhs));
                return;
            }
            if (e->op == "<" && fits12(*rhs)) {
                genExprNoCall(e->lhs.get(), dst, regs);
                emit("slti " + dst + ", " + dst + ", " + to_string(*rhs));
                return;
            }
            if (e->op == ">=" && fits12(*rhs)) {
                genExprNoCall(e->lhs.get(), dst, regs);
                emit("slti " + dst + ", " + dst + ", " + to_string(*rhs));
                emit("xori " + dst + ", " + dst + ", 1");
                return;
            }
            if (e->op == "==" && *rhs == 0) {
                genExprNoCall(e->lhs.get(), dst, regs);
                emit("sltiu " + dst + ", " + dst + ", 1");
                return;
            }
            if (e->op == "!=" && *rhs == 0) {
                genExprNoCall(e->lhs.get(), dst, regs);
                emit("sltu " + dst + ", x0, " + dst);
                return;
            }
            if ((e->op == "==" || e->op == "!=") && fits12(-*rhs)) {
                genExprNoCall(e->lhs.get(), dst, regs);
                emit("addi " + dst + ", " + dst + ", " + to_string(-*rhs));
                if (e->op == "==") emit("sltiu " + dst + ", " + dst + ", 1");
                else emit("sltu " + dst + ", x0, " + dst);
                return;
            }
            if (e->op == "*" && *rhs == 0) {
                emit("li " + dst + ", 0");
                return;
            }
            if (e->op == "*" && *rhs == 1) {
                genExprNoCall(e->lhs.get(), dst, regs);
                return;
            }
            if (e->op == "*" && isPowerOfTwo(*rhs)) {
                genExprNoCall(e->lhs.get(), dst, regs);
                emit("slli " + dst + ", " + dst + ", " + to_string(log2Int(*rhs)));
                return;
            }
            if (e->op == "/" && *rhs == 1) {
                genExprNoCall(e->lhs.get(), dst, regs);
                return;
            }
            if (e->op == "%" && *rhs == 1) {
                emit("li " + dst + ", 0");
                return;
            }
        }
        if (auto lhs = tryConst(e->lhs.get())) {
            if (e->op == "+" && *lhs == 0) {
                genExprNoCall(e->rhs.get(), dst, regs);
                return;
            }
            if (e->op == "+" && fits12(*lhs)) {
                genExprNoCall(e->rhs.get(), dst, regs);
                emit("addi " + dst + ", " + dst + ", " + to_string(*lhs));
                return;
            }
            if (e->op == "-" && *lhs == 0) {
                genExprNoCall(e->rhs.get(), dst, regs);
                emit("sub " + dst + ", x0, " + dst);
                return;
            }
            if (e->op == "*" && *lhs == 0) {
                emit("li " + dst + ", 0");
                return;
            }
            if (e->op == "*" && *lhs == 1) {
                genExprNoCall(e->rhs.get(), dst, regs);
                return;
            }
            if (e->op == "*" && isPowerOfTwo(*lhs)) {
                genExprNoCall(e->rhs.get(), dst, regs);
                emit("slli " + dst + ", " + dst + ", " + to_string(log2Int(*lhs)));
                return;
            }
        }

        if (regs.empty()) {
            genExprNoCall(e->lhs.get(), dst, regs);
            pushReg(dst);
            genExprNoCall(e->rhs.get(), dst, {});
            popTo("t6");
            emitBinaryReg(e->op, dst, "t6", dst);
            return;
        }

        string lhsReg = regs.front();
        regs.erase(regs.begin());
        genExprNoCall(e->lhs.get(), lhsReg, regs);
        genExprNoCall(e->rhs.get(), dst, regs);
        emitBinaryReg(e->op, dst, lhsReg, dst);
    }

    void genCondFalse(const Expr *e, const string &falseLabel) {
        if (auto v = tryConst(e)) {
            if (!*v) emit("j " + falseLabel);
            return;
        }
        if (e->kind == Expr::Kind::Unary && e->op == "!") {
            genCondTrue(e->lhs.get(), falseLabel);
            return;
        }
        if (e->kind == Expr::Kind::Binary && e->op == "&&") {
            genCondFalse(e->lhs.get(), falseLabel);
            genCondFalse(e->rhs.get(), falseLabel);
            return;
        }
        if (e->kind == Expr::Kind::Binary && e->op == "||") {
            string trueLabel = newLabel("cond_true");
            genCondTrue(e->lhs.get(), trueLabel);
            genCondFalse(e->rhs.get(), falseLabel);
            emitLabel(trueLabel);
            return;
        }
        static const unordered_set<string> relops = {"<", ">", "<=", ">=", "==", "!="};
        if (e->kind == Expr::Kind::Binary && relops.count(e->op) && !hasCall(e)) {
            genExprNoCall(e->lhs.get(), "a0", {"t1", "t2", "t3", "t4", "t5"});
            genExprNoCall(e->rhs.get(), "t0", {"t1", "t2", "t3", "t4", "t5"});
            if (e->op == "<") emit("bge a0, t0, " + falseLabel);
            else if (e->op == ">") emit("bge t0, a0, " + falseLabel);
            else if (e->op == "<=") emit("blt t0, a0, " + falseLabel);
            else if (e->op == ">=") emit("blt a0, t0, " + falseLabel);
            else if (e->op == "==") emit("bne a0, t0, " + falseLabel);
            else if (e->op == "!=") emit("beq a0, t0, " + falseLabel);
            return;
        }
        genExpr(e);
        emit("beqz a0, " + falseLabel);
    }

    void genCondTrue(const Expr *e, const string &trueLabel) {
        if (auto v = tryConst(e)) {
            if (*v) emit("j " + trueLabel);
            return;
        }
        if (e->kind == Expr::Kind::Unary && e->op == "!") {
            genCondFalse(e->lhs.get(), trueLabel);
            return;
        }
        if (e->kind == Expr::Kind::Binary && e->op == "&&") {
            string falseLabel = newLabel("cond_false");
            genCondFalse(e->lhs.get(), falseLabel);
            genCondTrue(e->rhs.get(), trueLabel);
            emitLabel(falseLabel);
            return;
        }
        if (e->kind == Expr::Kind::Binary && e->op == "||") {
            genCondTrue(e->lhs.get(), trueLabel);
            genCondTrue(e->rhs.get(), trueLabel);
            return;
        }
        static const unordered_set<string> relops = {"<", ">", "<=", ">=", "==", "!="};
        if (e->kind == Expr::Kind::Binary && relops.count(e->op) && !hasCall(e)) {
            genExprNoCall(e->lhs.get(), "a0", {"t1", "t2", "t3", "t4", "t5"});
            genExprNoCall(e->rhs.get(), "t0", {"t1", "t2", "t3", "t4", "t5"});
            if (e->op == "<") emit("blt a0, t0, " + trueLabel);
            else if (e->op == ">") emit("blt t0, a0, " + trueLabel);
            else if (e->op == "<=") emit("bge t0, a0, " + trueLabel);
            else if (e->op == ">=") emit("bge a0, t0, " + trueLabel);
            else if (e->op == "==") emit("beq a0, t0, " + trueLabel);
            else if (e->op == "!=") emit("bne a0, t0, " + trueLabel);
            return;
        }
        genExpr(e);
        emit("bnez a0, " + trueLabel);
    }

    void genTailSelfCall(const Expr *e) {
        int n = static_cast<int>(e->args.size());
        if (n != static_cast<int>(currentParams.size())) {
            genCall(e);
            return;
        }
        for (int i = 0; i < n; ++i) {
            genExpr(e->args[i].get());
            pushA0();
        }
        for (int i = 0; i < n; ++i) {
            int tempOffset = 16 * (n - 1 - i) + 12;
            loadMem("a0", "sp", tempOffset);
            storeParamFrom(currentParams[i], "a0");
        }
        adjustSp(16 * n);
        emit("j " + functionBodyLabel);
    }

    void genBinary(const Expr *e) {
        if ((e->op == "+" || e->op == "-") && e->rhs) {
            if (auto rhs = tryConst(e->rhs.get()); rhs && fits12(e->op == "+" ? *rhs : -*rhs)) {
                genExpr(e->lhs.get());
                long long imm = e->op == "+" ? *rhs : -*rhs;
                emit("addi a0, a0, " + to_string(imm));
                return;
            }
        }

        if (e->rhs) {
            if (auto rhs = tryConst(e->rhs.get())) {
                if (e->op == "<" && fits12(*rhs)) {
                    genExpr(e->lhs.get());
                    emit("slti a0, a0, " + to_string(*rhs));
                    return;
                }
                if (e->op == ">=" && fits12(*rhs)) {
                    genExpr(e->lhs.get());
                    emit("slti a0, a0, " + to_string(*rhs));
                    emit("xori a0, a0, 1");
                    return;
                }
                if (e->op == "==" && *rhs == 0) {
                    genExpr(e->lhs.get());
                    emit("sltiu a0, a0, 1");
                    return;
                }
                if (e->op == "!=" && *rhs == 0) {
                    genExpr(e->lhs.get());
                    emit("sltu a0, x0, a0");
                    return;
                }
                if ((e->op == "==" || e->op == "!=") && fits12(-*rhs)) {
                    genExpr(e->lhs.get());
                    emit("addi a0, a0, " + to_string(-*rhs));
                    if (e->op == "==") emit("sltiu a0, a0, 1");
                    else emit("sltu a0, x0, a0");
                    return;
                }
                if (e->op == "*" && *rhs == 0) {
                    emit("li a0, 0");
                    return;
                }
                if (e->op == "*" && *rhs == 1) {
                    genExpr(e->lhs.get());
                    return;
                }
                if (e->op == "/" && *rhs == 1) {
                    genExpr(e->lhs.get());
                    return;
                }
                if (e->op == "%" && *rhs == 1) {
                    emit("li a0, 0");
                    return;
                }
            }
        }
        if (e->lhs) {
            if (auto lhs = tryConst(e->lhs.get())) {
                if (e->op == "<") {
                    genExpr(e->rhs.get());
                    emit("li t0, " + to_string(*lhs));
                    emit("slt a0, t0, a0");
                    return;
                }
                if (e->op == ">") {
                    genExpr(e->rhs.get());
                    emit("li t0, " + to_string(*lhs));
                    emit("slt a0, a0, t0");
                    return;
                }
                if (e->op == "<=") {
                    genExpr(e->rhs.get());
                    emit("li t0, " + to_string(*lhs));
                    emit("slt a0, a0, t0");
                    emit("xori a0, a0, 1");
                    return;
                }
                if (e->op == ">=") {
                    genExpr(e->rhs.get());
                    emit("li t0, " + to_string(*lhs));
                    emit("slt a0, t0, a0");
                    emit("xori a0, a0, 1");
                    return;
                }
                if (e->op == "*" && *lhs == 0) {
                    emit("li a0, 0");
                    return;
                }
                if (e->op == "*" && *lhs == 1) {
                    genExpr(e->rhs.get());
                    return;
                }
            }
        }
        if (e->op == "+" && e->lhs) {
            if (auto lhs = tryConst(e->lhs.get()); lhs && fits12(*lhs)) {
                genExpr(e->rhs.get());
                emit("addi a0, a0, " + to_string(*lhs));
                return;
            }
        }

        if (e->op == "&&") {
            string falseLabel = newLabel("land_false");
            string endLabel = newLabel("land_end");
            genExpr(e->lhs.get());
            emit("beqz a0, " + falseLabel);
            genExpr(e->rhs.get());
            emit("sltu a0, x0, a0");
            emit("j " + endLabel);
            emitLabel(falseLabel);
            emit("li a0, 0");
            emitLabel(endLabel);
            return;
        }
        if (e->op == "||") {
            string trueLabel = newLabel("lor_true");
            string endLabel = newLabel("lor_end");
            genExpr(e->lhs.get());
            emit("bnez a0, " + trueLabel);
            genExpr(e->rhs.get());
            emit("sltu a0, x0, a0");
            emit("j " + endLabel);
            emitLabel(trueLabel);
            emit("li a0, 1");
            emitLabel(endLabel);
            return;
        }

        genExpr(e->lhs.get());
        pushA0();
        genExpr(e->rhs.get());
        popTo("t0");

        if (e->op == "+") emit("add a0, t0, a0");
        else if (e->op == "-") emit("sub a0, t0, a0");
        else if (e->op == "*") emit("mul a0, t0, a0");
        else if (e->op == "/") emit("div a0, t0, a0");
        else if (e->op == "%") emit("rem a0, t0, a0");
        else if (e->op == "<") emit("slt a0, t0, a0");
        else if (e->op == ">") emit("slt a0, a0, t0");
        else if (e->op == "<=") {
            emit("slt a0, a0, t0");
            emit("xori a0, a0, 1");
        } else if (e->op == ">=") {
            emit("slt a0, t0, a0");
            emit("xori a0, a0, 1");
        } else if (e->op == "==") {
            emit("sub a0, t0, a0");
            emit("sltiu a0, a0, 1");
        } else if (e->op == "!=") {
            emit("sub a0, t0, a0");
            emit("sltu a0, x0, a0");
        }
    }

    void genCall(const Expr *e) {
        int n = static_cast<int>(e->args.size());
        for (int i = 0; i < n; ++i) {
            genExpr(e->args[i].get());
            pushA0();
        }
        int extra = max(0, n - 8);
        int extraBytes = alignTo(extra * 4, 16);
        if (extraBytes) adjustSp(-extraBytes);
        for (int i = 0; i < n; ++i) {
            int tempOffset = extraBytes + 16 * (n - 1 - i) + 12;
            loadMem("t0", "sp", tempOffset);
            if (i < 8) emit("mv a" + to_string(i) + ", t0");
            else storeMem("t0", "sp", (i - 8) * 4);
        }
        emit("call " + e->name);
        adjustSp(extraBytes + 16 * n);
    }

    void loadVar(const string &name) {
        auto sym = lookup(name);
        if (!sym) {
            emit("li a0, 0");
            return;
        }
        if (sym->isConst) {
            emit("li a0, " + to_string(sym->constValue));
        } else if (!sym->reg.empty()) {
            if (sym->reg != "a0") emit("mv a0, " + sym->reg);
        } else if (sym->isGlobal) {
            emit("la t0, " + sym->label);
            loadMem("a0", "t0", 0);
        } else {
            loadMem("a0", "s0", sym->offset);
        }
    }

    void storeVar(const string &name) {
        auto sym = lookup(name);
        if (!sym || sym->isConst) return;
        if (!sym->reg.empty()) {
            if (sym->reg != "a0") emit("mv " + sym->reg + ", a0");
        } else if (sym->isGlobal) {
            emit("la t0, " + sym->label);
            storeMem("a0", "t0", 0);
        } else {
            storeMem("a0", "s0", sym->offset);
        }
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string input((istreambuf_iterator<char>(cin)), istreambuf_iterator<char>());
    Lexer lexer(input);
    auto tokens = lexer.lex();
    Parser parser(std::move(tokens));
    Program program = parser.parseProgram();
    Resolver resolver(program);
    resolver.run();
    Optimizer optimizer(program);
    optimizer.run();
    CodeGen codegen(program);
    cout << codegen.generate();
    return 0;
}
