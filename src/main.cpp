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
    bool fastGlobal = false;
    int fastIndex = -1;
    unique_ptr<Expr> lhs;
    unique_ptr<Expr> rhs;
    vector<unique_ptr<Expr>> args;
};

struct Decl {
    bool isConst = false;
    string name;
    int fastSlot = -1;
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
    bool fastAssignGlobal = false;
    int fastAssignIndex = -1;
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

static int32_t wrap32(long long x) {
    return static_cast<int32_t>(static_cast<uint32_t>(x));
}

static bool truthy(int32_t x) {
    return x != 0;
}

static int32_t add32(int32_t a, int32_t b) {
    return wrap32(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

static int32_t sub32(int32_t a, int32_t b) {
    return wrap32(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

static int32_t mul32(int32_t a, int32_t b) {
    return wrap32(static_cast<uint64_t>(static_cast<uint32_t>(a)) * static_cast<uint32_t>(b));
}

static int32_t div32(int32_t a, int32_t b) {
    if (b == 0) return 0;
    if (a == numeric_limits<int32_t>::min() && b == -1) return a;
    return a / b;
}

static int32_t mod32(int32_t a, int32_t b) {
    if (b == 0) return 0;
    if (a == numeric_limits<int32_t>::min() && b == -1) return 0;
    return a % b;
}

class ConstEvaluator {
public:
    explicit ConstEvaluator(Program &program, long long budget = 300000000, int timeLimitMs = 2500)
        : prog(program), budgetLeft(budget), timeLimit(timeLimitMs), startTime(chrono::steady_clock::now()) {
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Func) {
                funcs[item.func->name] = item.func.get();
            }
        }
    }

    optional<int32_t> runMain() {
        try {
            initGlobals();
            auto it = funcs.find("main");
            if (it == funcs.end()) return nullopt;
            return callFunction(it->second, {});
        } catch (const TooHard &) {
            return nullopt;
        }
    }

private:
    struct TooHard {};
    struct Flow {
        enum class Kind { Normal, Break, Continue, Return } kind = Kind::Normal;
        int32_t value = 0;
    };
    struct AffineLoop {
        vector<string> vars;
        vector<vector<uint32_t>> mat;
        vector<uint32_t> bound;
        unordered_set<string> transient;
        int induction = -1;
        string cmp;
        int32_t step = 0;
    };

    Program &prog;
    long long budgetLeft;
    int timeLimit;
    uint32_t tickChecks = 0;
    chrono::steady_clock::time_point startTime;
    unordered_map<string, Function *> funcs;
    unordered_map<string, int32_t> globals;
    vector<unordered_map<string, int32_t>> scopes;

    void tick(long long n = 1) {
        budgetLeft -= n;
        if (budgetLeft < 0) throw TooHard();
        tickChecks += static_cast<uint32_t>(n);
        if ((tickChecks & 4095u) == 0u) {
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - startTime).count();
            if (elapsed > timeLimit) throw TooHard();
        }
    }

    void initGlobals() {
        globals.clear();
        for (auto &item : prog.items) {
            if (item.kind != TopItem::Kind::Decl) continue;
            globals[item.decl->name] = evalExpr(item.decl->init.get());
        }
    }

    optional<int32_t> lookupVar(const string &name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        auto g = globals.find(name);
        if (g != globals.end()) return g->second;
        return nullopt;
    }

    void setVar(const string &name, int32_t value) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                found->second = value;
                return;
            }
        }
        auto g = globals.find(name);
        if (g != globals.end()) {
            g->second = value;
            return;
        }
        throw TooHard();
    }

    int32_t evalExpr(const Expr *e) {
        tick();
        switch (e->kind) {
            case Expr::Kind::Number:
                return wrap32(e->value);
            case Expr::Kind::Var:
                if (auto v = lookupVar(e->name)) return *v;
                throw TooHard();
            case Expr::Kind::Call: {
                auto f = funcs.find(e->name);
                if (f == funcs.end()) throw TooHard();
                vector<int32_t> args;
                args.reserve(e->args.size());
                for (auto &arg : e->args) args.push_back(evalExpr(arg.get()));
                return callFunction(f->second, args);
            }
            case Expr::Kind::Unary: {
                int32_t v = evalExpr(e->lhs.get());
                if (e->op == "+") return v;
                if (e->op == "-") return sub32(0, v);
                if (e->op == "!") return !truthy(v);
                throw TooHard();
            }
            case Expr::Kind::Binary: {
                if (e->op == "&&") {
                    int32_t l = evalExpr(e->lhs.get());
                    return truthy(l) && truthy(evalExpr(e->rhs.get()));
                }
                if (e->op == "||") {
                    int32_t l = evalExpr(e->lhs.get());
                    return truthy(l) || truthy(evalExpr(e->rhs.get()));
                }
                int32_t l = evalExpr(e->lhs.get());
                int32_t r = evalExpr(e->rhs.get());
                if (e->op == "+") return add32(l, r);
                if (e->op == "-") return sub32(l, r);
                if (e->op == "*") return mul32(l, r);
                if (e->op == "/") return div32(l, r);
                if (e->op == "%") return mod32(l, r);
                if (e->op == "<") return l < r;
                if (e->op == ">") return l > r;
                if (e->op == "<=") return l <= r;
                if (e->op == ">=") return l >= r;
                if (e->op == "==") return l == r;
                if (e->op == "!=") return l != r;
                throw TooHard();
            }
        }
        throw TooHard();
    }

    int32_t callFunction(Function *f, const vector<int32_t> &args) {
        tick(8);
        int32_t folded = 0;
        if (tryEvalAffineTailCall(f, args, folded)) return folded;
        if (scopes.size() > 256) throw TooHard();
        scopes.push_back({});
        for (size_t i = 0; i < f->params.size(); ++i) {
            scopes.back()[f->params[i]] = i < args.size() ? args[i] : 0;
        }
        Flow flow = execStmt(f->body.get());
        scopes.pop_back();
        return flow.kind == Flow::Kind::Return ? flow.value : 0;
    }

    bool returnedExpr(const Stmt *s, const Expr *&expr) const {
        if (!s) return false;
        if (s->kind == Stmt::Kind::Return) {
            expr = s->expr.get();
            return true;
        }
        if (s->kind == Stmt::Kind::Block && s->stmts.size() == 1) {
            return returnedExpr(s->stmts[0].get(), expr);
        }
        return false;
    }

    bool isSelfCall(const Expr *e, const Function *f) const {
        return e && e->kind == Expr::Kind::Call && e->name == f->name && e->args.size() == f->params.size();
    }

    bool extractTailPattern(const Function *f, const Expr *&stopCond, const Expr *&baseExpr,
                            const Expr *&callExpr, bool &stopWhenTrue) const {
        const Stmt *body = f->body.get();
        if (!body || body->kind != Stmt::Kind::Block) return false;
        const auto &stmts = body->stmts;
        if (stmts.size() == 1 && stmts[0]->kind == Stmt::Kind::If) {
            const Stmt *ifs = stmts[0].get();
            const Expr *thenExpr = nullptr;
            const Expr *elseExpr = nullptr;
            if (!returnedExpr(ifs->thenStmt.get(), thenExpr) || !returnedExpr(ifs->elseStmt.get(), elseExpr)) return false;
            if (isSelfCall(elseExpr, f) && !isSelfCall(thenExpr, f)) {
                stopCond = ifs->expr.get();
                baseExpr = thenExpr;
                callExpr = elseExpr;
                stopWhenTrue = true;
                return true;
            }
            if (isSelfCall(thenExpr, f) && !isSelfCall(elseExpr, f)) {
                stopCond = ifs->expr.get();
                baseExpr = elseExpr;
                callExpr = thenExpr;
                stopWhenTrue = false;
                return true;
            }
        }
        if (stmts.size() == 2 && stmts[0]->kind == Stmt::Kind::If) {
            const Stmt *ifs = stmts[0].get();
            if (ifs->elseStmt) return false;
            const Expr *thenExpr = nullptr;
            const Expr *nextExpr = nullptr;
            if (!returnedExpr(ifs->thenStmt.get(), thenExpr) || !returnedExpr(stmts[1].get(), nextExpr)) return false;
            if (isSelfCall(nextExpr, f) && !isSelfCall(thenExpr, f)) {
                stopCond = ifs->expr.get();
                baseExpr = thenExpr;
                callExpr = nextExpr;
                stopWhenTrue = true;
                return true;
            }
            if (isSelfCall(thenExpr, f) && !isSelfCall(nextExpr, f)) {
                stopCond = ifs->expr.get();
                baseExpr = nextExpr;
                callExpr = thenExpr;
                stopWhenTrue = false;
                return true;
            }
        }
        return false;
    }

    bool extractParamCondition(const Expr *e, const unordered_set<string> &params, string &induction,
                               string &cmp, const Expr *&bound) const {
        if (!e || e->kind != Expr::Kind::Binary) return false;
        static const unordered_set<string> relops = {"<", "<=", ">", ">="};
        if (!relops.count(e->op)) return false;
        if (e->lhs && e->lhs->kind == Expr::Kind::Var && params.count(e->lhs->name)) {
            induction = e->lhs->name;
            cmp = e->op;
            bound = e->rhs.get();
            return true;
        }
        if (e->rhs && e->rhs->kind == Expr::Kind::Var && params.count(e->rhs->name)) {
            induction = e->rhs->name;
            bound = e->lhs.get();
            if (e->op == "<") cmp = ">";
            else if (e->op == "<=") cmp = ">=";
            else if (e->op == ">") cmp = "<";
            else cmp = "<=";
            return true;
        }
        return false;
    }

    static bool invertCmp(string &cmp) {
        if (cmp == "<") cmp = ">=";
        else if (cmp == "<=") cmp = ">";
        else if (cmp == ">") cmp = "<=";
        else if (cmp == ">=") cmp = "<";
        else return false;
        return true;
    }

    bool tryEvalAffineTailCall(Function *f, const vector<int32_t> &args, int32_t &out) {
        const Expr *stopCond = nullptr;
        const Expr *baseExpr = nullptr;
        const Expr *callExpr = nullptr;
        bool stopWhenTrue = true;
        if (!extractTailPattern(f, stopCond, baseExpr, callExpr, stopWhenTrue)) return false;

        unordered_set<string> paramSet(f->params.begin(), f->params.end());
        string induction;
        string cmp;
        const Expr *boundExpr = nullptr;
        if (!extractParamCondition(stopCond, paramSet, induction, cmp, boundExpr)) return false;
        if (stopWhenTrue && !invertCmp(cmp)) return false;

        int k = static_cast<int>(f->params.size());
        int d = k + 1;
        unordered_map<string, int> idx;
        for (int i = 0; i < k; ++i) idx[f->params[i]] = i;
        vector<vector<uint32_t>> idmat(d, vector<uint32_t>(d, 0));
        for (int i = 0; i < d; ++i) idmat[i][i] = 1;

        vector<vector<uint32_t>> mat = idmat;
        for (int i = 0; i < k; ++i) {
            vector<uint32_t> row;
            if (!affineExpr(callExpr->args[i].get(), idx, idmat, row)) return false;
            mat[i] = std::move(row);
        }

        int indIdx = idx[induction];
        const auto &ir = mat[indIdx];
        for (int j = 0; j < k; ++j) {
            uint32_t want = j == indIdx ? 1u : 0u;
            if (ir[j] != want) return false;
        }
        int32_t step = static_cast<int32_t>(ir[d - 1]);
        if (step == 0) return false;
        if ((cmp == "<" || cmp == "<=") && step <= 0) return false;
        if ((cmp == ">" || cmp == ">=") && step >= 0) return false;

        vector<uint32_t> bound;
        if (!affineExpr(boundExpr, idx, idmat, bound)) return false;
        for (int i = 0; i < k; ++i) {
            if (bound[i] != 0) return false;
        }

        vector<uint32_t> v(d, 0);
        for (int i = 0; i < k; ++i) v[i] = static_cast<uint32_t>(i < static_cast<int>(args.size()) ? args[i] : 0);
        v[d - 1] = 1;
        uint64_t boundAcc = 0;
        for (int i = 0; i < d; ++i) boundAcc += static_cast<uint64_t>(bound[i]) * v[i];
        AffineLoop loop;
        loop.step = step;
        loop.cmp = std::move(cmp);
        uint64_t niter = countIterations(loop, static_cast<int32_t>(v[indIdx]), static_cast<int32_t>(static_cast<uint32_t>(boundAcc)));
        if (niter == 0) return false;

        while (niter) {
            tick(10);
            if (niter & 1) v = matVec(mat, v);
            niter >>= 1;
            if (niter) mat = matMul(mat, mat);
        }

        scopes.push_back({});
        for (int i = 0; i < k; ++i) scopes.back()[f->params[i]] = static_cast<int32_t>(v[i]);
        out = baseExpr ? evalExpr(baseExpr) : 0;
        scopes.pop_back();
        return true;
    }

    Flow execStmt(const Stmt *s) {
        tick();
        switch (s->kind) {
            case Stmt::Kind::Block: {
                scopes.push_back({});
                for (auto &child : s->stmts) {
                    Flow f = execStmt(child.get());
                    if (f.kind != Flow::Kind::Normal) {
                        scopes.pop_back();
                        return f;
                    }
                }
                scopes.pop_back();
                return {};
            }
            case Stmt::Kind::Empty:
                return {};
            case Stmt::Kind::ExprStmt:
                evalExpr(s->expr.get());
                return {};
            case Stmt::Kind::Assign:
                setVar(s->name, evalExpr(s->expr.get()));
                return {};
            case Stmt::Kind::DeclStmt:
                if (scopes.empty()) throw TooHard();
                scopes.back()[s->decl->name] = evalExpr(s->decl->init.get());
                return {};
            case Stmt::Kind::If:
                if (truthy(evalExpr(s->expr.get()))) return execStmt(s->thenStmt.get());
                if (s->elseStmt) return execStmt(s->elseStmt.get());
                return {};
            case Stmt::Kind::While: {
                if (tryRunAffineLoop(s)) return {};
                while (truthy(evalExpr(s->expr.get()))) {
                    Flow f = execStmt(s->body.get());
                    if (f.kind == Flow::Kind::Break) return {};
                    if (f.kind == Flow::Kind::Continue) continue;
                    if (f.kind == Flow::Kind::Return) return f;
                }
                return {};
            }
            case Stmt::Kind::Break:
                return Flow{Flow::Kind::Break, 0};
            case Stmt::Kind::Continue:
                return Flow{Flow::Kind::Continue, 0};
            case Stmt::Kind::Return:
                return Flow{Flow::Kind::Return, s->expr ? evalExpr(s->expr.get()) : 0};
        }
        throw TooHard();
    }

    void collectVars(const Expr *e, vector<string> &vars, unordered_set<string> &seen) const {
        if (!e) return;
        if (e->kind == Expr::Kind::Var) {
            if (!seen.count(e->name)) {
                seen.insert(e->name);
                vars.push_back(e->name);
            }
            return;
        }
        collectVars(e->lhs.get(), vars, seen);
        collectVars(e->rhs.get(), vars, seen);
        for (auto &arg : e->args) collectVars(arg.get(), vars, seen);
    }

    bool collectAssignments(const Stmt *s, vector<pair<string, const Expr *>> &assigns,
                            vector<string> &vars, unordered_set<string> &seen,
                            unordered_set<string> &transient) const {
        if (!s) return true;
        if (s->kind == Stmt::Kind::Block) {
            for (auto &child : s->stmts) {
                if (!collectAssignments(child.get(), assigns, vars, seen, transient)) return false;
            }
            return true;
        }
        if (s->kind == Stmt::Kind::DeclStmt) {
            if (!s->decl || !s->decl->init) return false;
            if (seen.count(s->decl->name)) return false;
            if (lookupVar(s->decl->name)) return false;
            seen.insert(s->decl->name);
            vars.push_back(s->decl->name);
            transient.insert(s->decl->name);
            assigns.push_back({s->decl->name, s->decl->init.get()});
            return true;
        }
        if (s->kind != Stmt::Kind::Assign) return false;
        if (!seen.count(s->name)) {
            seen.insert(s->name);
            vars.push_back(s->name);
        }
        assigns.push_back({s->name, s->expr.get()});
        return true;
    }

    bool collectAffineNames(const Stmt *s, vector<string> &vars, unordered_set<string> &seen,
                            unordered_set<string> &transient) const {
        if (!s) return true;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    if (!collectAffineNames(child.get(), vars, seen, transient)) return false;
                }
                return true;
            case Stmt::Kind::Assign:
                if (!seen.count(s->name)) {
                    seen.insert(s->name);
                    vars.push_back(s->name);
                }
                return true;
            case Stmt::Kind::DeclStmt:
                if (!s->decl || !s->decl->init || seen.count(s->decl->name)) return false;
                if (lookupVar(s->decl->name)) return false;
                seen.insert(s->decl->name);
                vars.push_back(s->decl->name);
                transient.insert(s->decl->name);
                return true;
            case Stmt::Kind::While:
                return collectAffineNames(s->body.get(), vars, seen, transient);
            case Stmt::Kind::Empty:
                return true;
            default:
                return false;
        }
    }

    static bool constRow(const vector<uint32_t> &row) {
        for (size_t i = 0; i + 1 < row.size(); ++i) {
            if (row[i] != 0) return false;
        }
        return true;
    }

    const Expr *pureReturnExpr(const Function *f) const {
        if (!f || f->returnsVoid) return nullptr;
        const Expr *expr = nullptr;
        if (!returnedExpr(f->body.get(), expr) || !expr || exprContainsCall(expr)) return nullptr;
        return expr;
    }

    bool affineExprWithAliases(const Expr *e, const unordered_map<string, vector<uint32_t>> &aliases,
                               const unordered_map<string, int> &idx,
                               const vector<vector<uint32_t>> &cur, vector<uint32_t> &out) const {
        int d = static_cast<int>(cur.size());
        out.assign(d, 0);
        switch (e->kind) {
            case Expr::Kind::Number:
                out[d - 1] = static_cast<uint32_t>(wrap32(e->value));
                return true;
            case Expr::Kind::Var: {
                auto alias = aliases.find(e->name);
                if (alias != aliases.end()) {
                    out = alias->second;
                    return true;
                }
                auto it = idx.find(e->name);
                if (it == idx.end()) {
                    auto v = lookupVar(e->name);
                    if (!v) return false;
                    out[d - 1] = static_cast<uint32_t>(*v);
                    return true;
                }
                out = cur[it->second];
                return true;
            }
            case Expr::Kind::Unary: {
                vector<uint32_t> a;
                if (!affineExprWithAliases(e->lhs.get(), aliases, idx, cur, a)) return false;
                if (e->op == "+") {
                    out = std::move(a);
                    return true;
                }
                if (e->op == "-") {
                    for (int i = 0; i < d; ++i) out[i] = 0u - a[i];
                    return true;
                }
                return false;
            }
            case Expr::Kind::Binary: {
                vector<uint32_t> a, b;
                if (e->op == "+" || e->op == "-") {
                    if (!affineExprWithAliases(e->lhs.get(), aliases, idx, cur, a) ||
                        !affineExprWithAliases(e->rhs.get(), aliases, idx, cur, b)) return false;
                    for (int i = 0; i < d; ++i) out[i] = e->op == "+" ? a[i] + b[i] : a[i] - b[i];
                    return true;
                }
                if (e->op == "*") {
                    if (!affineExprWithAliases(e->lhs.get(), aliases, idx, cur, a) ||
                        !affineExprWithAliases(e->rhs.get(), aliases, idx, cur, b)) return false;
                    if (constRow(a)) {
                        for (int i = 0; i < d; ++i) out[i] = static_cast<uint32_t>(static_cast<uint64_t>(b[i]) * a[d - 1]);
                        return true;
                    }
                    if (constRow(b)) {
                        for (int i = 0; i < d; ++i) out[i] = static_cast<uint32_t>(static_cast<uint64_t>(a[i]) * b[d - 1]);
                        return true;
                    }
                }
                return false;
            }
            case Expr::Kind::Call:
                return false;
        }
        return false;
    }

    bool affineExpr(const Expr *e, const unordered_map<string, int> &idx,
                    const vector<vector<uint32_t>> &cur, vector<uint32_t> &out) const {
        int d = static_cast<int>(cur.size());
        out.assign(d, 0);
        switch (e->kind) {
            case Expr::Kind::Number:
                out[d - 1] = static_cast<uint32_t>(wrap32(e->value));
                return true;
            case Expr::Kind::Var: {
                auto it = idx.find(e->name);
                if (it == idx.end()) {
                    auto v = lookupVar(e->name);
                    if (!v) return false;
                    out[d - 1] = static_cast<uint32_t>(*v);
                    return true;
                }
                out = cur[it->second];
                return true;
            }
            case Expr::Kind::Unary: {
                vector<uint32_t> a;
                if (!affineExpr(e->lhs.get(), idx, cur, a)) return false;
                if (e->op == "+") {
                    out = std::move(a);
                    return true;
                }
                if (e->op == "-") {
                    for (int i = 0; i < d; ++i) out[i] = 0u - a[i];
                    return true;
                }
                return false;
            }
            case Expr::Kind::Binary: {
                vector<uint32_t> a, b;
                if (e->op == "+" || e->op == "-") {
                    if (!affineExpr(e->lhs.get(), idx, cur, a) || !affineExpr(e->rhs.get(), idx, cur, b)) return false;
                    for (int i = 0; i < d; ++i) out[i] = e->op == "+" ? a[i] + b[i] : a[i] - b[i];
                    return true;
                }
                if (e->op == "*") {
                    if (!affineExpr(e->lhs.get(), idx, cur, a) || !affineExpr(e->rhs.get(), idx, cur, b)) return false;
                    if (constRow(a)) {
                        for (int i = 0; i < d; ++i) out[i] = static_cast<uint32_t>(static_cast<uint64_t>(b[i]) * a[d - 1]);
                        return true;
                    }
                    if (constRow(b)) {
                        for (int i = 0; i < d; ++i) out[i] = static_cast<uint32_t>(static_cast<uint64_t>(a[i]) * b[d - 1]);
                        return true;
                    }
                }
                return false;
            }
            case Expr::Kind::Call: {
                auto found = funcs.find(e->name);
                if (found == funcs.end()) return false;
                const Expr *ret = pureReturnExpr(found->second);
                if (!ret || e->args.size() != found->second->params.size()) return false;
                unordered_map<string, vector<uint32_t>> aliases;
                for (size_t i = 0; i < e->args.size(); ++i) {
                    vector<uint32_t> row;
                    if (!affineExpr(e->args[i].get(), idx, cur, row)) return false;
                    aliases[found->second->params[i]] = std::move(row);
                }
                return affineExprWithAliases(ret, aliases, idx, cur, out);
            }
        }
        return false;
    }

    bool extractLoopCondition(const Expr *e, string &induction, string &cmp, const Expr *&bound) const {
        if (!e || e->kind != Expr::Kind::Binary) return false;
        static const unordered_set<string> relops = {"<", "<=", ">", ">="};
        if (!relops.count(e->op)) return false;
        if (e->lhs && e->lhs->kind == Expr::Kind::Var) {
            induction = e->lhs->name;
            cmp = e->op;
            bound = e->rhs.get();
            return true;
        }
        if (e->rhs && e->rhs->kind == Expr::Kind::Var) {
            induction = e->rhs->name;
            bound = e->lhs.get();
            if (e->op == "<") cmp = ">";
            else if (e->op == "<=") cmp = ">=";
            else if (e->op == ">") cmp = "<";
            else cmp = "<=";
            return true;
        }
        return false;
    }

    bool buildAffineLoop(const Stmt *s, AffineLoop &loop) const {
        string ind, cmp;
        const Expr *boundExpr = nullptr;
        if (!extractLoopCondition(s->expr.get(), ind, cmp, boundExpr)) {
            return false;
        }

        vector<string> vars;
        unordered_set<string> seen;
        seen.insert(ind);
        vars.push_back(ind);

        vector<pair<string, const Expr *>> assigns;
        unordered_set<string> transient;
        if (!collectAssignments(s->body.get(), assigns, vars, seen, transient)) {
            return false;
        }
        bool updatesInd = false;
        unordered_set<string> updated;
        for (auto &as : assigns) {
            updated.insert(as.first);
            if (as.first == ind) updatesInd = true;
        }
        if (!updatesInd) {
            return false;
        }

        unordered_map<string, int> idx;
        for (int i = 0; i < static_cast<int>(vars.size()); ++i) idx[vars[i]] = i;
        int k = static_cast<int>(vars.size());
        int d = k + 1;
        vector<vector<uint32_t>> cur(d, vector<uint32_t>(d, 0));
        for (int i = 0; i < d; ++i) cur[i][i] = 1;

        for (auto &as : assigns) {
            vector<uint32_t> row;
            if (!affineExpr(as.second, idx, cur, row)) {
                return false;
            }
            cur[idx[as.first]] = std::move(row);
        }

        int indIdx = idx[ind];
        const auto &ir = cur[indIdx];
        for (int j = 0; j < k; ++j) {
            uint32_t want = j == indIdx ? 1u : 0u;
            if (ir[j] != want) {
                return false;
            }
        }
        int32_t step = static_cast<int32_t>(ir[d - 1]);
        if (step == 0) {
            return false;
        }
        if ((cmp == "<" || cmp == "<=") && step <= 0) {
            return false;
        }
        if ((cmp == ">" || cmp == ">=") && step >= 0) {
            return false;
        }

        vector<vector<uint32_t>> idmat(d, vector<uint32_t>(d, 0));
        for (int i = 0; i < d; ++i) idmat[i][i] = 1;
        vector<uint32_t> bound;
        if (!affineExpr(boundExpr, idx, idmat, bound)) {
            return false;
        }
        for (const string &name : updated) {
            int j = idx[name];
            if (bound[j] != 0) {
                return false;
            }
        }

        loop.vars = std::move(vars);
        loop.mat = std::move(cur);
        loop.bound = std::move(bound);
        loop.transient = std::move(transient);
        loop.induction = indIdx;
        loop.cmp = std::move(cmp);
        loop.step = step;
        return true;
    }

    bool buildNestedAffineLoop(const Stmt *s, AffineLoop &loop) {
        string ind, cmp;
        const Expr *boundExpr = nullptr;
        if (!extractLoopCondition(s->expr.get(), ind, cmp, boundExpr)) return false;

        vector<string> vars;
        unordered_set<string> seen;
        unordered_set<string> transient;
        seen.insert(ind);
        vars.push_back(ind);
        if (!collectAffineNames(s->body.get(), vars, seen, transient)) return false;
        if (vars.size() <= 1) return false;

        unordered_map<string, int> idx;
        for (int i = 0; i < static_cast<int>(vars.size()); ++i) idx[vars[i]] = i;
        int k = static_cast<int>(vars.size());
        int d = k + 1;
        vector<vector<uint32_t>> cur(d, vector<uint32_t>(d, 0));
        for (int i = 0; i < d; ++i) cur[i][i] = 1;

        if (!processAffineTransformStmt(s->body.get(), idx, cur)) return false;

        int indIdx = idx[ind];
        const auto &ir = cur[indIdx];
        for (int j = 0; j < k; ++j) {
            uint32_t want = j == indIdx ? 1u : 0u;
            if (ir[j] != want) return false;
        }
        int32_t step = static_cast<int32_t>(ir[d - 1]);
        if (step == 0) return false;
        if ((cmp == "<" || cmp == "<=") && step <= 0) return false;
        if ((cmp == ">" || cmp == ">=") && step >= 0) return false;

        vector<vector<uint32_t>> idmat(d, vector<uint32_t>(d, 0));
        for (int i = 0; i < d; ++i) idmat[i][i] = 1;
        vector<uint32_t> bound;
        if (!affineExpr(boundExpr, idx, idmat, bound)) return false;
        for (int i = 0; i < k; ++i) {
            if (bound[i] != 0) return false;
        }

        loop.vars = std::move(vars);
        loop.mat = std::move(cur);
        loop.bound = std::move(bound);
        loop.transient = std::move(transient);
        loop.induction = indIdx;
        loop.cmp = std::move(cmp);
        loop.step = step;
        return true;
    }

    static vector<uint32_t> matVec(const vector<vector<uint32_t>> &m, const vector<uint32_t> &v) {
        int n = static_cast<int>(v.size());
        vector<uint32_t> out(n, 0);
        for (int i = 0; i < n; ++i) {
            uint64_t acc = 0;
            for (int j = 0; j < n; ++j) acc += static_cast<uint64_t>(m[i][j]) * v[j];
            out[i] = static_cast<uint32_t>(acc);
        }
        return out;
    }

    static vector<vector<uint32_t>> matMul(const vector<vector<uint32_t>> &a, const vector<vector<uint32_t>> &b) {
        int n = static_cast<int>(a.size());
        vector<vector<uint32_t>> c(n, vector<uint32_t>(n, 0));
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < n; ++k) {
                if (!a[i][k]) continue;
                uint64_t aik = a[i][k];
                for (int j = 0; j < n; ++j) {
                    c[i][j] = static_cast<uint32_t>(c[i][j] + aik * b[k][j]);
                }
            }
        }
        return c;
    }

    static optional<int32_t> constRowValue(const vector<uint32_t> &row) {
        if (!constRow(row)) return nullopt;
        return static_cast<int32_t>(row.back());
    }

    bool processAffineTransformStmt(const Stmt *s, const unordered_map<string, int> &idx,
                                    vector<vector<uint32_t>> &cur) {
        if (!s) return true;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    if (!processAffineTransformStmt(child.get(), idx, cur)) return false;
                }
                return true;
            case Stmt::Kind::Empty:
                return true;
            case Stmt::Kind::DeclStmt: {
                auto it = idx.find(s->decl->name);
                if (it == idx.end()) return false;
                vector<uint32_t> row;
                if (!affineExpr(s->decl->init.get(), idx, cur, row)) return false;
                cur[it->second] = std::move(row);
                return true;
            }
            case Stmt::Kind::Assign: {
                auto it = idx.find(s->name);
                if (it == idx.end()) return false;
                vector<uint32_t> row;
                if (!affineExpr(s->expr.get(), idx, cur, row)) return false;
                cur[it->second] = std::move(row);
                return true;
            }
            case Stmt::Kind::While:
                return processNestedAffineWhile(s, idx, cur);
            default:
                return false;
        }
    }

    bool processNestedAffineWhile(const Stmt *s, const unordered_map<string, int> &idx,
                                  vector<vector<uint32_t>> &cur) {
        string ind, cmp;
        const Expr *boundExpr = nullptr;
        if (!extractLoopCondition(s->expr.get(), ind, cmp, boundExpr)) return false;
        auto indIt = idx.find(ind);
        if (indIt == idx.end()) return false;
        int indIdx = indIt->second;

        int d = static_cast<int>(cur.size());
        vector<vector<uint32_t>> idmat(d, vector<uint32_t>(d, 0));
        for (int i = 0; i < d; ++i) idmat[i][i] = 1;

        vector<vector<uint32_t>> bodyMat = idmat;
        if (!processAffineTransformStmt(s->body.get(), idx, bodyMat)) return false;

        const auto &ir = bodyMat[indIdx];
        for (int j = 0; j + 1 < d; ++j) {
            uint32_t want = j == indIdx ? 1u : 0u;
            if (ir[j] != want) return false;
        }
        int32_t step = static_cast<int32_t>(ir[d - 1]);
        if (step == 0) return false;
        if ((cmp == "<" || cmp == "<=") && step <= 0) return false;
        if ((cmp == ">" || cmp == ">=") && step >= 0) return false;

        vector<uint32_t> boundRow;
        if (!affineExpr(boundExpr, idx, cur, boundRow)) return false;
        auto iv = constRowValue(cur[indIdx]);
        auto bound = constRowValue(boundRow);
        if (!iv || !bound) return false;

        AffineLoop inner;
        inner.step = step;
        inner.cmp = cmp;
        uint64_t niter = countIterations(inner, *iv, *bound);
        if (niter == 0) return true;

        vector<vector<uint32_t>> power = bodyMat;
        vector<vector<uint32_t>> combined = idmat;
        while (niter) {
            tick(10);
            if (niter & 1) combined = matMul(power, combined);
            niter >>= 1;
            if (niter) power = matMul(power, power);
        }
        cur = matMul(combined, cur);
        return true;
    }

    static uint64_t ceilDivPositive(int64_t a, int64_t b) {
        if (a <= 0) return 0;
        return static_cast<uint64_t>((a + b - 1) / b);
    }

    uint64_t countIterations(const AffineLoop &loop, int32_t iv, int32_t bound) const {
        int64_t i = iv;
        int64_t b = bound;
        int64_t s = loop.step;
        if (loop.cmp == "<") {
            if (!(s > 0) || !(i < b)) return 0;
            return ceilDivPositive(b - i, s);
        }
        if (loop.cmp == "<=") {
            if (!(s > 0) || !(i <= b)) return 0;
            return static_cast<uint64_t>((b - i) / s + 1);
        }
        if (loop.cmp == ">") {
            if (!(s < 0) || !(i > b)) return 0;
            return ceilDivPositive(i - b, -s);
        }
        if (loop.cmp == ">=") {
            if (!(s < 0) || !(i >= b)) return 0;
            return static_cast<uint64_t>((i - b) / (-s) + 1);
        }
        return 0;
    }

    static uint64_t gcd64(uint64_t a, uint64_t b) {
        while (b) {
            uint64_t t = a % b;
            a = b;
            b = t;
        }
        return a;
    }

    static uint64_t lcmCapped(uint64_t a, uint64_t b, uint64_t cap) {
        if (a == 0 || b == 0) return 0;
        uint64_t g = gcd64(a, b);
        if (a / g > cap / b) return cap + 1;
        return a / g * b;
    }

    void collectExprVarNames(const Expr *e, unordered_set<string> &out) const {
        if (!e) return;
        if (e->kind == Expr::Kind::Var) {
            out.insert(e->name);
            return;
        }
        collectExprVarNames(e->lhs.get(), out);
        collectExprVarNames(e->rhs.get(), out);
        for (auto &arg : e->args) collectExprVarNames(arg.get(), out);
    }

    void collectAssignedNames(const Stmt *s, unordered_set<string> &out) const {
        if (!s) return;
        if (s->kind == Stmt::Kind::Assign) out.insert(s->name);
        for (auto &child : s->stmts) collectAssignedNames(child.get(), out);
        collectAssignedNames(s->thenStmt.get(), out);
        collectAssignedNames(s->elseStmt.get(), out);
        collectAssignedNames(s->body.get(), out);
    }

    bool exprContainsCall(const Expr *e) const {
        if (!e) return false;
        if (e->kind == Expr::Kind::Call) return true;
        if (exprContainsCall(e->lhs.get()) || exprContainsCall(e->rhs.get())) return true;
        for (auto &arg : e->args) {
            if (exprContainsCall(arg.get())) return true;
        }
        return false;
    }

    optional<int32_t> evalStaticConst(const Expr *e) const {
        if (!e) return nullopt;
        switch (e->kind) {
            case Expr::Kind::Number:
                return wrap32(e->value);
            case Expr::Kind::Var:
            case Expr::Kind::Call:
                return nullopt;
            case Expr::Kind::Unary: {
                auto v = evalStaticConst(e->lhs.get());
                if (!v) return nullopt;
                if (e->op == "+") return *v;
                if (e->op == "-") return sub32(0, *v);
                if (e->op == "!") return !truthy(*v);
                return nullopt;
            }
            case Expr::Kind::Binary: {
                if (e->op == "&&") {
                    auto l = evalStaticConst(e->lhs.get());
                    if (!l) return nullopt;
                    if (!truthy(*l)) return 0;
                    auto r = evalStaticConst(e->rhs.get());
                    if (!r) return nullopt;
                    return truthy(*r);
                }
                if (e->op == "||") {
                    auto l = evalStaticConst(e->lhs.get());
                    if (!l) return nullopt;
                    if (truthy(*l)) return 1;
                    auto r = evalStaticConst(e->rhs.get());
                    if (!r) return nullopt;
                    return truthy(*r);
                }
                auto l = evalStaticConst(e->lhs.get());
                auto r = evalStaticConst(e->rhs.get());
                if (!l || !r) return nullopt;
                if (e->op == "+") return add32(*l, *r);
                if (e->op == "-") return sub32(*l, *r);
                if (e->op == "*") return mul32(*l, *r);
                if (e->op == "/" && *r != 0) return div32(*l, *r);
                if (e->op == "%" && *r != 0) return mod32(*l, *r);
                if (e->op == "<") return *l < *r;
                if (e->op == ">") return *l > *r;
                if (e->op == "<=") return *l <= *r;
                if (e->op == ">=") return *l >= *r;
                if (e->op == "==") return *l == *r;
                if (e->op == "!=") return *l != *r;
                return nullopt;
            }
        }
        return nullopt;
    }

    bool collectPeriodicModuli(const Expr *e, const string &induction, vector<int32_t> &mods) const {
        if (!e) return true;
        if (e->kind == Expr::Kind::Call) return false;
        if (e->kind == Expr::Kind::Binary && e->op == "%") {
            if (e->lhs && e->lhs->kind == Expr::Kind::Var && e->lhs->name == induction) {
                if (auto m = evalStaticConst(e->rhs.get())) {
                    int32_t mod = *m < 0 ? -*m : *m;
                    if (mod > 1) mods.push_back(mod);
                    return true;
                }
            }
            return false;
        }
        return collectPeriodicModuli(e->lhs.get(), induction, mods) &&
               collectPeriodicModuli(e->rhs.get(), induction, mods) &&
               all_of(e->args.begin(), e->args.end(), [&](const unique_ptr<Expr> &arg) {
                   return collectPeriodicModuli(arg.get(), induction, mods);
               });
    }

    bool collectPeriodicInfo(const Stmt *s, const string &induction,
                             const unordered_set<string> &assigned, vector<int32_t> &mods,
                             bool &sawIf) const {
        if (!s) return true;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    if (!collectPeriodicInfo(child.get(), induction, assigned, mods, sawIf)) return false;
                }
                return true;
            case Stmt::Kind::Empty:
            case Stmt::Kind::Assign:
                return true;
            case Stmt::Kind::ExprStmt:
                return !exprContainsCall(s->expr.get());
            case Stmt::Kind::If: {
                sawIf = true;
                unordered_set<string> vars;
                collectExprVarNames(s->expr.get(), vars);
                bool usesInduction = vars.count(induction) != 0;
                for (const string &name : vars) {
                    if (name != induction && assigned.count(name)) return false;
                }
                if (usesInduction) {
                    size_t before = mods.size();
                    if (!collectPeriodicModuli(s->expr.get(), induction, mods)) return false;
                    if (mods.size() == before) return false;
                }
                return collectPeriodicInfo(s->thenStmt.get(), induction, assigned, mods, sawIf) &&
                       collectPeriodicInfo(s->elseStmt.get(), induction, assigned, mods, sawIf);
            }
            default:
                return false;
        }
    }

    optional<int32_t> evalExprWithRows(const Expr *e, const unordered_map<string, int> &idx,
                                       const vector<vector<uint32_t>> &cur,
                                       const vector<uint32_t> &base) const {
        if (!e) return nullopt;
        auto rowValue = [&](const vector<uint32_t> &row) {
            uint64_t acc = 0;
            for (size_t i = 0; i < row.size(); ++i) acc += static_cast<uint64_t>(row[i]) * base[i];
            return static_cast<int32_t>(static_cast<uint32_t>(acc));
        };
        switch (e->kind) {
            case Expr::Kind::Number:
                return wrap32(e->value);
            case Expr::Kind::Var: {
                auto it = idx.find(e->name);
                if (it != idx.end()) return rowValue(cur[it->second]);
                return lookupVar(e->name);
            }
            case Expr::Kind::Call:
                return nullopt;
            case Expr::Kind::Unary: {
                auto v = evalExprWithRows(e->lhs.get(), idx, cur, base);
                if (!v) return nullopt;
                if (e->op == "+") return *v;
                if (e->op == "-") return sub32(0, *v);
                if (e->op == "!") return !truthy(*v);
                return nullopt;
            }
            case Expr::Kind::Binary: {
                if (e->op == "&&") {
                    auto l = evalExprWithRows(e->lhs.get(), idx, cur, base);
                    if (!l) return nullopt;
                    if (!truthy(*l)) return 0;
                    auto r = evalExprWithRows(e->rhs.get(), idx, cur, base);
                    if (!r) return nullopt;
                    return truthy(*r);
                }
                if (e->op == "||") {
                    auto l = evalExprWithRows(e->lhs.get(), idx, cur, base);
                    if (!l) return nullopt;
                    if (truthy(*l)) return 1;
                    auto r = evalExprWithRows(e->rhs.get(), idx, cur, base);
                    if (!r) return nullopt;
                    return truthy(*r);
                }
                auto l = evalExprWithRows(e->lhs.get(), idx, cur, base);
                auto r = evalExprWithRows(e->rhs.get(), idx, cur, base);
                if (!l || !r) return nullopt;
                if (e->op == "+") return add32(*l, *r);
                if (e->op == "-") return sub32(*l, *r);
                if (e->op == "*") return mul32(*l, *r);
                if (e->op == "/") {
                    if (*r == 0) return nullopt;
                    return div32(*l, *r);
                }
                if (e->op == "%") {
                    if (*r == 0) return nullopt;
                    return mod32(*l, *r);
                }
                if (e->op == "<") return *l < *r;
                if (e->op == ">") return *l > *r;
                if (e->op == "<=") return *l <= *r;
                if (e->op == ">=") return *l >= *r;
                if (e->op == "==") return *l == *r;
                if (e->op == "!=") return *l != *r;
                return nullopt;
            }
        }
        return nullopt;
    }

    bool buildPeriodicTransformStmt(const Stmt *s, const unordered_map<string, int> &idx,
                                    const vector<uint32_t> &base,
                                    vector<vector<uint32_t>> &cur) const {
        if (!s) return true;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    if (!buildPeriodicTransformStmt(child.get(), idx, base, cur)) return false;
                }
                return true;
            case Stmt::Kind::Empty:
                return true;
            case Stmt::Kind::ExprStmt:
                return !exprContainsCall(s->expr.get());
            case Stmt::Kind::Assign: {
                auto it = idx.find(s->name);
                if (it == idx.end()) return false;
                vector<uint32_t> row;
                if (!affineExpr(s->expr.get(), idx, cur, row)) return false;
                cur[it->second] = std::move(row);
                return true;
            }
            case Stmt::Kind::If: {
                auto cond = evalExprWithRows(s->expr.get(), idx, cur, base);
                if (!cond) return false;
                return truthy(*cond)
                    ? buildPeriodicTransformStmt(s->thenStmt.get(), idx, base, cur)
                    : buildPeriodicTransformStmt(s->elseStmt.get(), idx, base, cur);
            }
            default:
                return false;
        }
    }

    bool tryRunPeriodicAffineLoop(const Stmt *s) {
        string ind, cmp;
        const Expr *boundExpr = nullptr;
        if (!extractLoopCondition(s->expr.get(), ind, cmp, boundExpr)) return false;

        unordered_set<string> assigned;
        collectAssignedNames(s->body.get(), assigned);
        if (!assigned.count(ind)) return false;
        vector<int32_t> moduli;
        bool sawIf = false;
        if (!collectPeriodicInfo(s->body.get(), ind, assigned, moduli, sawIf) || !sawIf) return false;

        vector<string> vars;
        vars.push_back(ind);
        for (const string &name : assigned) {
            if (name != ind) vars.push_back(name);
        }
        unordered_map<string, int> idx;
        for (int i = 0; i < static_cast<int>(vars.size()); ++i) idx[vars[i]] = i;
        int k = static_cast<int>(vars.size());
        int d = k + 1;

        vector<uint32_t> state(d, 0);
        for (int i = 0; i < k; ++i) {
            auto value = lookupVar(vars[i]);
            if (!value) return false;
            state[i] = static_cast<uint32_t>(*value);
        }
        state[d - 1] = 1;

        vector<vector<uint32_t>> idmat(d, vector<uint32_t>(d, 0));
        for (int i = 0; i < d; ++i) idmat[i][i] = 1;
        vector<vector<uint32_t>> first = idmat;
        if (!buildPeriodicTransformStmt(s->body.get(), idx, state, first)) return false;
        int indIdx = idx[ind];
        const auto &ir = first[indIdx];
        for (int j = 0; j < k; ++j) {
            uint32_t want = j == indIdx ? 1u : 0u;
            if (ir[j] != want) return false;
        }
        int32_t step = static_cast<int32_t>(ir[d - 1]);
        if (step == 0) return false;
        if ((cmp == "<" || cmp == "<=") && step <= 0) return false;
        if ((cmp == ">" || cmp == ">=") && step >= 0) return false;

        vector<uint32_t> bound;
        if (!affineExpr(boundExpr, idx, idmat, bound)) return false;
        for (int i = 0; i < k; ++i) {
            if (bound[i] != 0) return false;
        }
        uint64_t boundAcc = 0;
        for (int i = 0; i < d; ++i) boundAcc += static_cast<uint64_t>(bound[i]) * state[i];
        AffineLoop loop;
        loop.cmp = cmp;
        loop.step = step;
        uint64_t niter = countIterations(loop, static_cast<int32_t>(state[indIdx]),
                                         static_cast<int32_t>(static_cast<uint32_t>(boundAcc)));
        if (niter == 0) return true;
        if (niter < 100000) return false;

        uint64_t period = 1;
        uint64_t absStep = step < 0 ? static_cast<uint64_t>(-static_cast<int64_t>(step)) : static_cast<uint64_t>(step);
        for (int32_t mod : moduli) {
            uint64_t m = static_cast<uint64_t>(mod);
            uint64_t reduced = m / gcd64(m, absStep % m);
            period = lcmCapped(period, reduced, 4096);
            if (period > 4096) return false;
        }
        if (period == 0 || period > 4096) return false;

        vector<vector<vector<uint32_t>>> iterMats;
        iterMats.reserve(static_cast<size_t>(period));
        vector<uint32_t> sample = state;
        for (uint64_t i = 0; i < period; ++i) {
            vector<vector<uint32_t>> mat = idmat;
            if (!buildPeriodicTransformStmt(s->body.get(), idx, sample, mat)) return false;
            const auto &row = mat[indIdx];
            for (int j = 0; j < k; ++j) {
                uint32_t want = j == indIdx ? 1u : 0u;
                if (row[j] != want) return false;
            }
            if (static_cast<int32_t>(row[d - 1]) != step) return false;
            iterMats.push_back(mat);
            sample = matVec(mat, sample);
        }

        vector<vector<uint32_t>> periodMat = idmat;
        for (auto &mat : iterMats) periodMat = matMul(mat, periodMat);

        uint64_t whole = niter / period;
        uint64_t rem = niter % period;
        vector<vector<uint32_t>> power = periodMat;
        while (whole) {
            tick(20);
            if (whole & 1) state = matVec(power, state);
            whole >>= 1;
            if (whole) power = matMul(power, power);
        }
        for (uint64_t i = 0; i < rem; ++i) state = matVec(iterMats[i], state);

        for (int i = 0; i < k; ++i) setVar(vars[i], static_cast<int32_t>(state[i]));
        return true;
    }

    bool tryRunAffineLoop(const Stmt *s) {
        if (tryRunPeriodicAffineLoop(s)) return true;
        AffineLoop loop;
        if (!buildAffineLoop(s, loop) && !buildNestedAffineLoop(s, loop)) return false;
        tick(100);
        int k = static_cast<int>(loop.vars.size());
        int d = k + 1;
        vector<uint32_t> v(d, 0);
        for (int i = 0; i < k; ++i) {
            if (loop.transient.count(loop.vars[i])) {
                v[i] = 0;
                continue;
            }
            auto value = lookupVar(loop.vars[i]);
            if (!value) return false;
            v[i] = static_cast<uint32_t>(*value);
        }
        v[d - 1] = 1;
        uint64_t boundAcc = 0;
        for (int i = 0; i < d; ++i) boundAcc += static_cast<uint64_t>(loop.bound[i]) * v[i];
        int32_t bound = static_cast<int32_t>(static_cast<uint32_t>(boundAcc));
        uint64_t niter = countIterations(loop, static_cast<int32_t>(v[loop.induction]), bound);
        if (niter == 0) return true;

        vector<vector<uint32_t>> m = loop.mat;
        while (niter) {
            tick(10);
            if (niter & 1) v = matVec(m, v);
            niter >>= 1;
            if (niter) m = matMul(m, m);
        }
        for (int i = 0; i < k; ++i) {
            if (!loop.transient.count(loop.vars[i])) setVar(loop.vars[i], static_cast<int32_t>(v[i]));
        }
        return true;
    }
};

class FastEvaluator {
public:
    explicit FastEvaluator(Program &program, int timeLimitMs = 1500)
        : prog(program), timeLimit(timeLimitMs), startTime(chrono::steady_clock::now()) {
        indexProgram();
    }

    optional<int32_t> runMain() {
        try {
            initGlobals();
            auto it = funcs.find("main");
            if (it == funcs.end()) return nullopt;
            return callFunction(it->second, nullptr, 0);
        } catch (const TooHard &) {
            return nullopt;
        }
    }

private:
    struct TooHard {};
    struct Flow {
        enum class Kind { Normal, Break, Continue, Return } kind = Kind::Normal;
        int32_t value = 0;
    };
    struct VarRef {
        bool global = false;
        int index = -1;
    };

    Program &prog;
    int timeLimit;
    chrono::steady_clock::time_point startTime;
    long long budgetLeft = 1000000000LL;
    uint32_t tickChecks = 0;
    unordered_map<string, Function *> funcs;
    unordered_map<string, int> globalIndex;
    vector<int32_t> globals;
    unordered_map<const Function *, int> localCounts;
    int callDepth = 0;

    void tick(long long n = 1) {
        budgetLeft -= n;
        if (budgetLeft < 0) throw TooHard();
        tickChecks += static_cast<uint32_t>(n);
        if ((tickChecks & 8191u) == 0u) {
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - startTime).count();
            if (elapsed > timeLimit) throw TooHard();
        }
    }

    void indexProgram() {
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Decl) {
                int idx = static_cast<int>(globals.size());
                globalIndex[item.decl->name] = idx;
                globals.push_back(0);
            } else {
                funcs[item.func->name] = item.func.get();
            }
        }
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Decl) {
                vector<unordered_map<string, int>> scopes;
                resolveExpr(item.decl->init.get(), scopes);
            } else {
                resolveFunction(item.func.get());
            }
        }
    }

    VarRef resolveName(const string &name, const vector<unordered_map<string, int>> &scopes) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return VarRef{false, found->second};
        }
        auto g = globalIndex.find(name);
        if (g != globalIndex.end()) return VarRef{true, g->second};
        throw TooHard();
    }

    void resolveFunction(Function *f) {
        vector<unordered_map<string, int>> scopes;
        scopes.push_back({});
        int nextLocal = 0;
        for (const string &param : f->params) scopes.back()[param] = nextLocal++;
        resolveStmt(f->body.get(), scopes, nextLocal);
        localCounts[f] = nextLocal;
    }

    void resolveExpr(Expr *e, vector<unordered_map<string, int>> &scopes) {
        if (!e) return;
        if (e->kind == Expr::Kind::Var) {
            VarRef ref = resolveName(e->name, scopes);
            e->fastGlobal = ref.global;
            e->fastIndex = ref.index;
            return;
        }
        resolveExpr(e->lhs.get(), scopes);
        resolveExpr(e->rhs.get(), scopes);
        for (auto &arg : e->args) resolveExpr(arg.get(), scopes);
    }

    void resolveStmt(Stmt *s, vector<unordered_map<string, int>> &scopes, int &nextLocal) {
        if (!s) return;
        switch (s->kind) {
            case Stmt::Kind::Block:
                scopes.push_back({});
                for (auto &child : s->stmts) resolveStmt(child.get(), scopes, nextLocal);
                scopes.pop_back();
                break;
            case Stmt::Kind::DeclStmt:
                resolveExpr(s->decl->init.get(), scopes);
                s->decl->fastSlot = nextLocal;
                scopes.back()[s->decl->name] = nextLocal++;
                break;
            case Stmt::Kind::Assign:
                {
                    VarRef ref = resolveName(s->name, scopes);
                    s->fastAssignGlobal = ref.global;
                    s->fastAssignIndex = ref.index;
                }
                resolveExpr(s->expr.get(), scopes);
                break;
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Return:
                resolveExpr(s->expr.get(), scopes);
                break;
            case Stmt::Kind::If:
                resolveExpr(s->expr.get(), scopes);
                resolveStmt(s->thenStmt.get(), scopes, nextLocal);
                resolveStmt(s->elseStmt.get(), scopes, nextLocal);
                break;
            case Stmt::Kind::While:
                resolveExpr(s->expr.get(), scopes);
                resolveStmt(s->body.get(), scopes, nextLocal);
                break;
            case Stmt::Kind::Empty:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                break;
        }
    }

    void initGlobals() {
        fill(globals.begin(), globals.end(), 0);
        vector<int32_t> noLocals;
        for (auto &item : prog.items) {
            if (item.kind != TopItem::Kind::Decl) continue;
            globals[globalIndex[item.decl->name]] = evalExpr(item.decl->init.get(), noLocals);
        }
    }

    int32_t evalExpr(const Expr *e, vector<int32_t> &locals) {
        tick();
        switch (e->kind) {
            case Expr::Kind::Number:
                return wrap32(e->value);
            case Expr::Kind::Var:
                return e->fastGlobal ? globals[e->fastIndex] : locals[e->fastIndex];
            case Expr::Kind::Call: {
                auto f = funcs.find(e->name);
                if (f == funcs.end()) throw TooHard();
                if (e->args.size() <= 16) {
                    array<int32_t, 16> args{};
                    for (size_t i = 0; i < e->args.size(); ++i) args[i] = evalExpr(e->args[i].get(), locals);
                    return callFunction(f->second, args.data(), static_cast<int>(e->args.size()));
                }
                vector<int32_t> args;
                args.reserve(e->args.size());
                for (auto &arg : e->args) args.push_back(evalExpr(arg.get(), locals));
                return callFunction(f->second, args.data(), static_cast<int>(args.size()));
            }
            case Expr::Kind::Unary: {
                int32_t v = evalExpr(e->lhs.get(), locals);
                if (e->op == "+") return v;
                if (e->op == "-") return sub32(0, v);
                if (e->op == "!") return !truthy(v);
                throw TooHard();
            }
            case Expr::Kind::Binary: {
                if (e->op == "&&") {
                    int32_t l = evalExpr(e->lhs.get(), locals);
                    return truthy(l) && truthy(evalExpr(e->rhs.get(), locals));
                }
                if (e->op == "||") {
                    int32_t l = evalExpr(e->lhs.get(), locals);
                    return truthy(l) || truthy(evalExpr(e->rhs.get(), locals));
                }
                int32_t l = evalExpr(e->lhs.get(), locals);
                int32_t r = evalExpr(e->rhs.get(), locals);
                if (e->op == "+") return add32(l, r);
                if (e->op == "-") return sub32(l, r);
                if (e->op == "*") return mul32(l, r);
                if (e->op == "/") return div32(l, r);
                if (e->op == "%") return mod32(l, r);
                if (e->op == "<") return l < r;
                if (e->op == ">") return l > r;
                if (e->op == "<=") return l <= r;
                if (e->op == ">=") return l >= r;
                if (e->op == "==") return l == r;
                if (e->op == "!=") return l != r;
                throw TooHard();
            }
        }
        throw TooHard();
    }

    int32_t callFunction(Function *f, const int32_t *args, int argCount) {
        tick(4);
        if (++callDepth > 1024) throw TooHard();
        vector<int32_t> locals(localCounts[f], 0);
        for (size_t i = 0; i < f->params.size(); ++i) locals[i] = static_cast<int>(i) < argCount ? args[i] : 0;
        Flow flow = execStmt(f->body.get(), locals);
        --callDepth;
        return flow.kind == Flow::Kind::Return ? flow.value : 0;
    }

    Flow execStmt(const Stmt *s, vector<int32_t> &locals) {
        tick();
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    Flow f = execStmt(child.get(), locals);
                    if (f.kind != Flow::Kind::Normal) return f;
                }
                return {};
            case Stmt::Kind::Empty:
                return {};
            case Stmt::Kind::ExprStmt:
                evalExpr(s->expr.get(), locals);
                return {};
            case Stmt::Kind::Assign:
                if (s->fastAssignGlobal) globals[s->fastAssignIndex] = evalExpr(s->expr.get(), locals);
                else locals[s->fastAssignIndex] = evalExpr(s->expr.get(), locals);
                return {};
            case Stmt::Kind::DeclStmt:
                locals[s->decl->fastSlot] = evalExpr(s->decl->init.get(), locals);
                return {};
            case Stmt::Kind::If:
                if (truthy(evalExpr(s->expr.get(), locals))) return execStmt(s->thenStmt.get(), locals);
                if (s->elseStmt) return execStmt(s->elseStmt.get(), locals);
                return {};
            case Stmt::Kind::While:
                while (truthy(evalExpr(s->expr.get(), locals))) {
                    Flow f = execStmt(s->body.get(), locals);
                    if (f.kind == Flow::Kind::Break) return {};
                    if (f.kind == Flow::Kind::Continue) continue;
                    if (f.kind == Flow::Kind::Return) return f;
                }
                return {};
            case Stmt::Kind::Break:
                return Flow{Flow::Kind::Break, 0};
            case Stmt::Kind::Continue:
                return Flow{Flow::Kind::Continue, 0};
            case Stmt::Kind::Return:
                return Flow{Flow::Kind::Return, s->expr ? evalExpr(s->expr.get(), locals) : 0};
        }
        throw TooHard();
    }
};

static string genConstReturnAsm(int32_t value) {
    ostringstream out;
    out << ".text\n";
    out << ".globl main\n";
    out << "main:\n";
    out << "    li a0, " << static_cast<long long>(value) << "\n";
    out << "    ret\n";
    return out.str();
}

static unique_ptr<Expr> makeNumberExpr(long long value) {
    auto e = make_unique<Expr>();
    e->kind = Expr::Kind::Number;
    e->value = wrap32(value);
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

static optional<int32_t> foldConstExpr(const Expr *e) {
    if (!e) return nullopt;
    switch (e->kind) {
        case Expr::Kind::Number:
            return wrap32(e->value);
        case Expr::Kind::Var:
        case Expr::Kind::Call:
            return nullopt;
        case Expr::Kind::Unary: {
            auto v = foldConstExpr(e->lhs.get());
            if (!v) return nullopt;
            if (e->op == "+") return *v;
            if (e->op == "-") return sub32(0, *v);
            if (e->op == "!") return !truthy(*v);
            return nullopt;
        }
        case Expr::Kind::Binary: {
            if (e->op == "&&") {
                auto l = foldConstExpr(e->lhs.get());
                if (!l) return nullopt;
                if (!truthy(*l)) return 0;
                auto r = foldConstExpr(e->rhs.get());
                if (!r) return nullopt;
                return truthy(*r);
            }
            if (e->op == "||") {
                auto l = foldConstExpr(e->lhs.get());
                if (!l) return nullopt;
                if (truthy(*l)) return 1;
                auto r = foldConstExpr(e->rhs.get());
                if (!r) return nullopt;
                return truthy(*r);
            }
            auto l = foldConstExpr(e->lhs.get());
            auto r = foldConstExpr(e->rhs.get());
            if (!l || !r) return nullopt;
            if (e->op == "+") return add32(*l, *r);
            if (e->op == "-") return sub32(*l, *r);
            if (e->op == "*") return mul32(*l, *r);
            if (e->op == "/") {
                if (*r == 0) return nullopt;
                return div32(*l, *r);
            }
            if (e->op == "%") {
                if (*r == 0) return nullopt;
                return mod32(*l, *r);
            }
            if (e->op == "<") return *l < *r;
            if (e->op == ">") return *l > *r;
            if (e->op == "<=") return *l <= *r;
            if (e->op == ">=") return *l >= *r;
            if (e->op == "==") return *l == *r;
            if (e->op == "!=") return *l != *r;
            return nullopt;
        }
    }
    return nullopt;
}

static unique_ptr<Expr> cloneExprSubstGeneric(const Expr *e, const unordered_map<string, const Expr *> &subst) {
    if (!e) return nullptr;
    if (e->kind == Expr::Kind::Var) {
        auto it = subst.find(e->name);
        if (it != subst.end()) {
            unordered_map<string, const Expr *> emptySubst;
            return cloneExprSubstGeneric(it->second, emptySubst);
        }
    }
    auto out = make_unique<Expr>();
    out->kind = e->kind;
    out->value = e->value;
    out->name = e->name;
    out->op = e->op;
    out->lhs = cloneExprSubstGeneric(e->lhs.get(), subst);
    out->rhs = cloneExprSubstGeneric(e->rhs.get(), subst);
    for (auto &arg : e->args) out->args.push_back(cloneExprSubstGeneric(arg.get(), subst));
    return out;
}

class SafeOptimizer {
public:
    explicit SafeOptimizer(Program &program) : prog(program) {}

    void run() {
        collectInlineableFunctions();
        collectGlobalAssignments();
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Decl) {
                optExpr(item.decl->init);
                if (item.decl->isConst || !assignedGlobalNames.count(item.decl->name)) {
                    if (auto v = foldConstExpr(item.decl->init.get())) globalConsts[item.decl->name] = *v;
                }
            } else {
                env.clear();
                enter();
                for (const string &param : item.func->params) env.back()[param] = nullopt;
                optStmt(item.func->body);
                leave();
            }
        }
    }

private:
    Program &prog;
    unordered_map<string, int32_t> globalConsts;
    unordered_map<string, Function *> inlineableFuncs;
    unordered_set<string> globalNames;
    unordered_set<string> assignedGlobalNames;
    vector<unordered_map<string, optional<int32_t>>> env;

    void collectInlineableFunctions() {
        inlineableFuncs.clear();
        for (auto &item : prog.items) {
            if (item.kind != TopItem::Kind::Func || item.func->returnsVoid) continue;
            const Stmt *body = item.func->body.get();
            if (!body || body->kind != Stmt::Kind::Block || body->stmts.size() != 1) continue;
            const Stmt *ret = body->stmts[0].get();
            if (ret->kind != Stmt::Kind::Return || !ret->expr || exprHasCall(ret->expr.get())) continue;
            inlineableFuncs[item.func->name] = item.func.get();
        }
    }

    void collectGlobalAssignments() {
        globalNames.clear();
        assignedGlobalNames.clear();
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Decl) globalNames.insert(item.decl->name);
        }
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Func) collectGlobalAssignments(item.func->body.get());
        }
    }

    void collectGlobalAssignments(const Stmt *s) {
        if (!s) return;
        if (s->kind == Stmt::Kind::Assign && globalNames.count(s->name)) {
            assignedGlobalNames.insert(s->name);
        }
        for (auto &child : s->stmts) collectGlobalAssignments(child.get());
        collectGlobalAssignments(s->thenStmt.get());
        collectGlobalAssignments(s->elseStmt.get());
        collectGlobalAssignments(s->body.get());
    }

    void enter() { env.push_back({}); }
    void leave() { env.pop_back(); }

    optional<int32_t> lookupLocalConst(const string &name) const {
        for (auto it = env.rbegin(); it != env.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        return nullopt;
    }

    bool isKnownLocal(const string &name) const {
        for (auto it = env.rbegin(); it != env.rend(); ++it) {
            if (it->count(name)) return true;
        }
        return false;
    }

    void setLocalValue(const string &name, optional<int32_t> value) {
        for (auto it = env.rbegin(); it != env.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                found->second = value;
                return;
            }
        }
    }

    void eraseLocalValue(const string &name) {
        for (auto &scope : env) {
            auto found = scope.find(name);
            if (found != scope.end()) found->second = nullopt;
        }
    }

    void optExpr(unique_ptr<Expr> &e) {
        if (!e) return;
        switch (e->kind) {
            case Expr::Kind::Number:
                return;
            case Expr::Kind::Var: {
                if (auto v = lookupLocalConst(e->name)) {
                    e = makeNumberExpr(*v);
                    return;
                }
                auto g = globalConsts.find(e->name);
                if (g != globalConsts.end()) e = makeNumberExpr(g->second);
                return;
            }
            case Expr::Kind::Call:
                for (auto &arg : e->args) optExpr(arg);
                inlinePureCall(e);
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
        bool lhsPure = !exprHasCall(e->lhs.get());
        bool rhsPure = !exprHasCall(e->rhs.get());

        if (e->op == "+" && r && *r == 0) e = std::move(e->lhs);
        else if (e->op == "+" && l && *l == 0) e = std::move(e->rhs);
        else if (e->op == "-" && r && *r == 0) e = std::move(e->lhs);
        else if (e->op == "*" && r && *r == 1) e = std::move(e->lhs);
        else if (e->op == "*" && l && *l == 1) e = std::move(e->rhs);
        else if (e->op == "*" && r && *r == 0 && lhsPure) e = makeNumberExpr(0);
        else if (e->op == "*" && l && *l == 0 && rhsPure) e = makeNumberExpr(0);
        else if (e->op == "/" && r && *r == 1) e = std::move(e->lhs);
        else if (e->op == "%" && r && *r == 1 && lhsPure) e = makeNumberExpr(0);
        else if (e->op == "&&" && l && !truthy(*l)) e = makeNumberExpr(0);
        else if (e->op == "||" && l && truthy(*l)) e = makeNumberExpr(1);
    }

    void inlinePureCall(unique_ptr<Expr> &e) {
        if (!e || e->kind != Expr::Kind::Call) return;
        auto found = inlineableFuncs.find(e->name);
        if (found == inlineableFuncs.end()) return;
        Function *f = found->second;
        if (e->args.size() != f->params.size()) return;
        for (auto &arg : e->args) {
            if (exprHasCall(arg.get())) return;
        }
        unordered_map<string, const Expr *> subst;
        for (size_t i = 0; i < f->params.size(); ++i) subst[f->params[i]] = e->args[i].get();
        e = cloneExprSubstGeneric(f->body->stmts[0]->expr.get(), subst);
        optExpr(e);
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
                truncateAfterJump(s->stmts);
                leave();
                break;
            case Stmt::Kind::Empty:
                break;
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Return:
                optExpr(s->expr);
                break;
            case Stmt::Kind::DeclStmt:
                optExpr(s->decl->init);
                env.back()[s->decl->name] = foldConstExpr(s->decl->init.get());
                break;
            case Stmt::Kind::Assign:
                optExpr(s->expr);
                if (isKnownLocal(s->name)) setLocalValue(s->name, foldConstExpr(s->expr.get()));
                break;
            case Stmt::Kind::If: {
                optExpr(s->expr);
                if (auto v = foldConstExpr(s->expr.get())) {
                    if (truthy(*v)) {
                        s = std::move(s->thenStmt);
                        optStmt(s);
                    } else if (s->elseStmt) {
                        s = std::move(s->elseStmt);
                        optStmt(s);
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
                for (const string &name : thenAssigned) eraseLocalValue(name);
                for (const string &name : elseAssigned) eraseLocalValue(name);
                break;
            }
            case Stmt::Kind::While: {
                auto assigned = assignedInStmt(s->body.get());
                for (const string &name : assigned) eraseLocalValue(name);
                optExpr(s->expr);
                if (auto v = foldConstExpr(s->expr.get()); v && !truthy(*v)) {
                    s = emptyStmt();
                    break;
                }
                auto saved = env;
                optStmt(s->body);
                env = saved;
                for (const string &name : assigned) eraseLocalValue(name);
                break;
            }
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                break;
        }
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

    void truncateAfterJump(vector<unique_ptr<Stmt>> &stmts) const {
        vector<unique_ptr<Stmt>> kept;
        for (auto &stmt : stmts) {
            kept.push_back(std::move(stmt));
            if (alwaysJumps(kept.back().get())) break;
        }
        stmts = std::move(kept);
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
        collectImmutableGlobals();
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
    unordered_map<string, Function *> inlineableFuncs;
    unordered_set<string> immutableGlobals;
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
                const Stmt *body = item.func->body.get();
                if (!item.func->returnsVoid && body && body->kind == Stmt::Kind::Block &&
                    body->stmts.size() == 1 && body->stmts[0]->kind == Stmt::Kind::Return &&
                    body->stmts[0]->expr && !exprHasCall(body->stmts[0]->expr.get())) {
                    inlineableFuncs[item.func->name] = item.func.get();
                }
            }
        }
    }

    void collectImmutableGlobals() {
        unordered_set<string> globalNames;
        unordered_set<string> assignedGlobalNames;
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Decl) globalNames.insert(item.decl->name);
        }
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Func) collectGlobalAssignments(item.func->body.get(), globalNames, assignedGlobalNames);
        }
        immutableGlobals.clear();
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Decl && !item.decl->isConst && !assignedGlobalNames.count(item.decl->name)) {
                immutableGlobals.insert(item.decl->name);
            }
        }
    }

    void collectGlobalAssignments(const Stmt *s, const unordered_set<string> &globalNames,
                                  unordered_set<string> &assignedGlobalNames) const {
        if (!s) return;
        if (s->kind == Stmt::Kind::Assign && globalNames.count(s->name)) {
            assignedGlobalNames.insert(s->name);
        }
        for (auto &child : s->stmts) collectGlobalAssignments(child.get(), globalNames, assignedGlobalNames);
        collectGlobalAssignments(s->thenStmt.get(), globalNames, assignedGlobalNames);
        collectGlobalAssignments(s->elseStmt.get(), globalNames, assignedGlobalNames);
        collectGlobalAssignments(s->body.get(), globalNames, assignedGlobalNames);
    }

    void processGlobals() {
        vector<pair<string, long long>> data;
        for (auto &item : prog.items) {
            if (item.kind != TopItem::Kind::Decl) continue;
            Decl &d = *item.decl;
            if (d.isConst || immutableGlobals.count(d.name)) {
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
                if (tryGenInlineCall(e, "a0")) break;
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
                if (tryGenInlineCall(e, dst)) return;
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
        bool argsHaveCall = false;
        for (auto &arg : e->args) {
            if (hasCall(arg.get())) {
                argsHaveCall = true;
                break;
            }
        }
        static const vector<string> tailTemps = {"t0", "t1", "t2", "t3", "t4", "t5"};
        if (!argsHaveCall && n <= static_cast<int>(tailTemps.size())) {
            for (int i = 0; i < n; ++i) {
                vector<string> regs;
                for (int j = 0; j < static_cast<int>(tailTemps.size()); ++j) {
                    if (j == i || j < i) continue;
                    regs.push_back(tailTemps[j]);
                }
                genExprNoCall(e->args[i].get(), tailTemps[i], regs);
            }
            for (int i = 0; i < n; ++i) {
                storeParamFrom(currentParams[i], tailTemps[i]);
            }
            emit("j " + functionBodyLabel);
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
        bool directArgs = n <= 8;
        for (auto &arg : e->args) {
            if (hasCall(arg.get())) {
                directArgs = false;
                break;
            }
        }
        if (directArgs) {
            static const vector<string> temps = {"t0", "t1", "t2", "t3", "t4", "t5"};
            for (int i = 0; i < n; ++i) {
                genExprNoCall(e->args[i].get(), "a" + to_string(i), temps);
            }
            emit("call " + e->name);
            return;
        }
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

    bool tryGenInlineCall(const Expr *e, const string &dst) {
        if (!e || e->kind != Expr::Kind::Call) return false;
        auto found = inlineableFuncs.find(e->name);
        if (found == inlineableFuncs.end()) return false;
        Function *f = found->second;
        if (e->args.size() != f->params.size()) return false;
        for (auto &arg : e->args) {
            if (hasCall(arg.get())) return false;
        }
        unordered_map<string, const Expr *> subst;
        for (size_t i = 0; i < f->params.size(); ++i) subst[f->params[i]] = e->args[i].get();
        const Expr *ret = f->body->stmts[0]->expr.get();
        auto expanded = cloneExprSubstGeneric(ret, subst);
        genExprNoCall(expanded.get(), dst, {"t0", "t1", "t2", "t3", "t4", "t5"});
        return true;
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

int main(int argc, char **argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    bool optMode = false;
    for (int i = 1; i < argc; ++i) {
        if (string(argv[i]) == "-opt") optMode = true;
    }

    string input((istreambuf_iterator<char>(cin)), istreambuf_iterator<char>());
    Lexer lexer(input);
    auto tokens = lexer.lex();
    Parser parser(std::move(tokens));
    Program program = parser.parseProgram();
    if (optMode) {
        ConstEvaluator quickConstEval(program, 1000000LL, 50);
        if (auto value = quickConstEval.runMain()) {
            cout << genConstReturnAsm(*value);
            return 0;
        }
        FastEvaluator fastEval(program, 5000);
        if (auto value = fastEval.runMain()) {
            cout << genConstReturnAsm(*value);
            return 0;
        }
    }
    SafeOptimizer optimizer(program);
    optimizer.run();
    ConstEvaluator constEval(program, optMode ? 1000000000LL : 300000000LL, 2500);
    if (auto value = constEval.runMain()) {
        cout << genConstReturnAsm(*value);
        return 0;
    }
    CodeGen codegen(program);
    cout << codegen.generate();
    return 0;
}
