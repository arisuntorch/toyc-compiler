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
    int opc = -1;
    bool rangeAnalyzed = false;
    int32_t rangeMin = numeric_limits<int32_t>::min();
    int32_t rangeMax = numeric_limits<int32_t>::max();
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
    bool fastDeadStore = false;
    bool fastLoopValuesDead = false;
    int fastLoopId = -1;
    unique_ptr<Expr> expr;
    unique_ptr<Stmt> thenStmt;
    unique_ptr<Stmt> elseStmt;
    unique_ptr<Stmt> body;
};

struct Function {
    bool returnsVoid = false;
    string name;
    vector<string> params;
    int fastLocalCount = -1;
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

enum : int {
    OPC_ADD = 0, OPC_SUB, OPC_MUL, OPC_DIV, OPC_MOD,
    OPC_LT, OPC_GT, OPC_LE, OPC_GE, OPC_EQ, OPC_NE,
    OPC_AND, OPC_OR, OPC_PLUS, OPC_NEG, OPC_NOT
};

// A deliberately small, static-only analysis pass.
//
// Keep backend metadata in an explicit static pass: lexical binding resolution,
// local slot assignment, operator tags, loop ids, and conservative dead-store
// analysis.  No ToyC statement or function is executed by this pass.
class StaticAnalyzer {
public:
    explicit StaticAnalyzer(Program &program) : prog(program) {}

    void run() {
        collectGlobals();
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Decl) {
                vector<unordered_map<string, int>> scopes;
                resolveExpr(item.decl->init.get(), scopes);
            } else {
                resolveFunction(item.func.get());
            }
        }
        for (auto &item : prog.items) {
            if (item.kind != TopItem::Kind::Func) continue;
            unordered_set<int> live;
            analyzeStmt(item.func->body.get(), live, nullptr, nullptr, true);
        }
        markDeadGlobalStores();
        // Global dead-store marks can make an enclosing local loop wholly
        // unobservable.  Recompute local liveness once with those stores
        // ignored so the loop optimizer can remove or summarize it.
        for (auto &item : prog.items) {
            if (item.kind != TopItem::Kind::Func) continue;
            unordered_set<int> live;
            analyzeStmt(item.func->body.get(), live, nullptr, nullptr, true);
        }
    }

private:
    struct Ref {
        bool global = false;
        int index = -1;
    };

    Program &prog;
    unordered_map<string, int> globalIndex;
    int loopCount = 0;

    static int encodeBinaryOp(const string &op) {
        if (op == "+") return OPC_ADD;
        if (op == "-") return OPC_SUB;
        if (op == "*") return OPC_MUL;
        if (op == "/") return OPC_DIV;
        if (op == "%") return OPC_MOD;
        if (op == "<") return OPC_LT;
        if (op == ">") return OPC_GT;
        if (op == "<=") return OPC_LE;
        if (op == ">=") return OPC_GE;
        if (op == "==") return OPC_EQ;
        if (op == "!=") return OPC_NE;
        if (op == "&&") return OPC_AND;
        if (op == "||") return OPC_OR;
        return -1;
    }

    static int encodeUnaryOp(const string &op) {
        if (op == "+") return OPC_PLUS;
        if (op == "-") return OPC_NEG;
        if (op == "!") return OPC_NOT;
        return -1;
    }

    void collectGlobals() {
        int index = 0;
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Decl) {
                globalIndex[item.decl->name] = index++;
            }
        }
    }

    Ref resolveName(const string &name,
                    const vector<unordered_map<string, int>> &scopes) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return Ref{false, found->second};
        }
        auto global = globalIndex.find(name);
        if (global != globalIndex.end()) return Ref{true, global->second};
        throw runtime_error("unresolved identifier: " + name);
    }

    void resolveFunction(Function *function) {
        vector<unordered_map<string, int>> scopes(1);
        int nextLocal = 0;
        for (const string &param : function->params) {
            scopes.back()[param] = nextLocal++;
        }
        resolveStmt(function->body.get(), scopes, nextLocal);
        function->fastLocalCount = nextLocal;
    }

    void resolveExpr(Expr *expr, vector<unordered_map<string, int>> &scopes) {
        if (!expr) return;
        if (expr->kind == Expr::Kind::Var) {
            Ref ref = resolveName(expr->name, scopes);
            expr->fastGlobal = ref.global;
            expr->fastIndex = ref.index;
            return;
        }
        if (expr->kind == Expr::Kind::Unary) expr->opc = encodeUnaryOp(expr->op);
        if (expr->kind == Expr::Kind::Binary) expr->opc = encodeBinaryOp(expr->op);
        resolveExpr(expr->lhs.get(), scopes);
        resolveExpr(expr->rhs.get(), scopes);
        for (auto &arg : expr->args) resolveExpr(arg.get(), scopes);
    }

    void resolveStmt(Stmt *stmt, vector<unordered_map<string, int>> &scopes,
                     int &nextLocal) {
        if (!stmt) return;
        // The analysis may be rerun after an AST rewrite.  Do not retain a
        // dead-store decision made for the pre-rewrite control-flow graph.
        stmt->fastDeadStore = false;
        stmt->fastLoopValuesDead = false;
        switch (stmt->kind) {
            case Stmt::Kind::Block:
                scopes.push_back({});
                for (auto &child : stmt->stmts) resolveStmt(child.get(), scopes, nextLocal);
                scopes.pop_back();
                return;
            case Stmt::Kind::DeclStmt:
                resolveExpr(stmt->decl->init.get(), scopes);
                stmt->decl->fastSlot = nextLocal;
                scopes.back()[stmt->decl->name] = nextLocal++;
                return;
            case Stmt::Kind::Assign: {
                Ref ref = resolveName(stmt->name, scopes);
                stmt->fastAssignGlobal = ref.global;
                stmt->fastAssignIndex = ref.index;
                resolveExpr(stmt->expr.get(), scopes);
                return;
            }
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Return:
                resolveExpr(stmt->expr.get(), scopes);
                return;
            case Stmt::Kind::If:
                resolveExpr(stmt->expr.get(), scopes);
                resolveStmt(stmt->thenStmt.get(), scopes, nextLocal);
                resolveStmt(stmt->elseStmt.get(), scopes, nextLocal);
                return;
            case Stmt::Kind::While:
                stmt->fastLoopId = loopCount++;
                resolveExpr(stmt->expr.get(), scopes);
                resolveStmt(stmt->body.get(), scopes, nextLocal);
                return;
            case Stmt::Kind::Empty:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return;
        }
    }

    static bool hasCall(const Expr *expr) {
        if (!expr) return false;
        if (expr->kind == Expr::Kind::Call) return true;
        if (hasCall(expr->lhs.get()) || hasCall(expr->rhs.get())) return true;
        for (auto &arg : expr->args) {
            if (hasCall(arg.get())) return true;
        }
        return false;
    }

    static void addReads(const Expr *expr, unordered_set<int> &live) {
        if (!expr) return;
        if (expr->kind == Expr::Kind::Var) {
            if (!expr->fastGlobal) live.insert(expr->fastIndex);
            return;
        }
        addReads(expr->lhs.get(), live);
        addReads(expr->rhs.get(), live);
        for (auto &arg : expr->args) addReads(arg.get(), live);
    }

    static bool loopWritesAreUnobservable(const Stmt *stmt,
                                          const unordered_set<int> &liveAfter) {
        if (!stmt || stmt->fastDeadStore) return true;
        switch (stmt->kind) {
            case Stmt::Kind::Block:
                for (auto &child : stmt->stmts) {
                    if (!loopWritesAreUnobservable(child.get(), liveAfter)) return false;
                }
                return true;
            case Stmt::Kind::Empty:
                return true;
            case Stmt::Kind::ExprStmt:
                return !hasCall(stmt->expr.get());
            case Stmt::Kind::Assign:
                return !stmt->fastAssignGlobal && !hasCall(stmt->expr.get()) &&
                       !liveAfter.count(stmt->fastAssignIndex);
            case Stmt::Kind::DeclStmt:
                return stmt->decl && !hasCall(stmt->decl->init.get()) &&
                       !liveAfter.count(stmt->decl->fastSlot);
            case Stmt::Kind::If:
                return !hasCall(stmt->expr.get()) &&
                       loopWritesAreUnobservable(stmt->thenStmt.get(), liveAfter) &&
                       loopWritesAreUnobservable(stmt->elseStmt.get(), liveAfter);
            case Stmt::Kind::While:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
            case Stmt::Kind::Return:
                return false;
        }
        return false;
    }

    // Backward data-flow analysis.  A local store can be removed only when its
    // value is not live and its right-hand side cannot call another function.
    // Loops are solved to a fixpoint; break and continue use their real target
    // live sets instead of being treated like ordinary fallthrough.
    void analyzeStmt(Stmt *stmt, unordered_set<int> &live,
                     const unordered_set<int> *breakLive,
                     const unordered_set<int> *continueLive, bool mark) const {
        if (!stmt) return;
        if (stmt->fastDeadStore &&
            (stmt->kind == Stmt::Kind::ExprStmt ||
             stmt->kind == Stmt::Kind::Assign ||
             stmt->kind == Stmt::Kind::DeclStmt)) {
            return;
        }
        switch (stmt->kind) {
            case Stmt::Kind::Block:
                for (auto it = stmt->stmts.rbegin(); it != stmt->stmts.rend(); ++it) {
                    analyzeStmt(it->get(), live, breakLive, continueLive, mark);
                }
                return;
            case Stmt::Kind::Empty:
                return;
            case Stmt::Kind::ExprStmt:
                if (hasCall(stmt->expr.get())) addReads(stmt->expr.get(), live);
                else if (mark) stmt->fastDeadStore = true;
                return;
            case Stmt::Kind::Return:
                live.clear();
                addReads(stmt->expr.get(), live);
                return;
            case Stmt::Kind::Break:
                if (breakLive) live = *breakLive;
                return;
            case Stmt::Kind::Continue:
                if (continueLive) live = *continueLive;
                return;
            case Stmt::Kind::If: {
                unordered_set<int> thenLive = live;
                unordered_set<int> elseLive = live;
                analyzeStmt(stmt->thenStmt.get(), thenLive, breakLive, continueLive, mark);
                analyzeStmt(stmt->elseStmt.get(), elseLive, breakLive, continueLive, mark);
                live = std::move(thenLive);
                live.insert(elseLive.begin(), elseLive.end());
                addReads(stmt->expr.get(), live);
                return;
            }
            case Stmt::Kind::While: {
                const unordered_set<int> after = live;
                if (mark) {
                    stmt->fastLoopValuesDead = !hasCall(stmt->expr.get()) &&
                        loopWritesAreUnobservable(stmt->body.get(), after);
                }
                unordered_set<int> head = after;
                addReads(stmt->expr.get(), head);
                for (int iteration = 0; iteration < 32; ++iteration) {
                    unordered_set<int> bodyLive = head;
                    analyzeStmt(stmt->body.get(), bodyLive, &after, &head, false);
                    unordered_set<int> next = after;
                    next.insert(bodyLive.begin(), bodyLive.end());
                    addReads(stmt->expr.get(), next);
                    if (next == head) break;
                    head = std::move(next);
                }
                if (mark) {
                    unordered_set<int> bodyLive = head;
                    analyzeStmt(stmt->body.get(), bodyLive, &after, &head, true);
                }
                live = std::move(head);
                return;
            }
            case Stmt::Kind::Assign: {
                if (stmt->fastAssignGlobal) {
                    addReads(stmt->expr.get(), live);
                    return;
                }
                int slot = stmt->fastAssignIndex;
                if (!live.count(slot) && !hasCall(stmt->expr.get())) {
                    if (mark) stmt->fastDeadStore = true;
                    return;
                }
                live.erase(slot);
                addReads(stmt->expr.get(), live);
                return;
            }
            case Stmt::Kind::DeclStmt: {
                int slot = stmt->decl->fastSlot;
                if (!live.count(slot) && !hasCall(stmt->decl->init.get())) {
                    if (mark) stmt->fastDeadStore = true;
                    return;
                }
                live.erase(slot);
                addReads(stmt->decl->init.get(), live);
                return;
            }
        }
    }

    struct GlobalStoreDependency {
        Stmt *stmt = nullptr;
        int target = -1;
        unordered_set<int> reads;
    };

    static void collectCalledNames(const Expr *expr,
                                   unordered_set<string> &called) {
        if (!expr) return;
        if (expr->kind == Expr::Kind::Call) called.insert(expr->name);
        collectCalledNames(expr->lhs.get(), called);
        collectCalledNames(expr->rhs.get(), called);
        for (auto &arg : expr->args) collectCalledNames(arg.get(), called);
    }

    static void collectCalledNames(const Stmt *stmt,
                                   unordered_set<string> &called) {
        if (!stmt || stmt->fastDeadStore) return;
        collectCalledNames(stmt->expr.get(), called);
        if (stmt->decl) collectCalledNames(stmt->decl->init.get(), called);
        for (auto &child : stmt->stmts) {
            collectCalledNames(child.get(), called);
        }
        collectCalledNames(stmt->thenStmt.get(), called);
        collectCalledNames(stmt->elseStmt.get(), called);
        collectCalledNames(stmt->body.get(), called);
    }

    static void collectGlobalReads(const Expr *expr,
                                   unordered_set<int> &reads) {
        if (!expr) return;
        if (expr->kind == Expr::Kind::Var && expr->fastGlobal &&
            expr->fastIndex >= 0) {
            reads.insert(expr->fastIndex);
            return;
        }
        collectGlobalReads(expr->lhs.get(), reads);
        collectGlobalReads(expr->rhs.get(), reads);
        for (auto &arg : expr->args) collectGlobalReads(arg.get(), reads);
    }

    static void collectGlobalStoreDependencies(
        Stmt *stmt, unordered_set<int> &roots,
        vector<GlobalStoreDependency> &stores) {
        if (!stmt || stmt->fastDeadStore) return;
        switch (stmt->kind) {
            case Stmt::Kind::Block:
                for (auto &child : stmt->stmts) {
                    collectGlobalStoreDependencies(child.get(), roots, stores);
                }
                return;
            case Stmt::Kind::Empty:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return;
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Return:
                collectGlobalReads(stmt->expr.get(), roots);
                return;
            case Stmt::Kind::DeclStmt:
                if (stmt->decl) collectGlobalReads(stmt->decl->init.get(), roots);
                return;
            case Stmt::Kind::Assign: {
                if (!stmt->fastAssignGlobal) {
                    collectGlobalReads(stmt->expr.get(), roots);
                    return;
                }
                unordered_set<int> reads;
                collectGlobalReads(stmt->expr.get(), reads);
                if (hasCall(stmt->expr.get())) {
                    roots.insert(reads.begin(), reads.end());
                    return;
                }
                stores.push_back(GlobalStoreDependency{
                    stmt, stmt->fastAssignIndex, std::move(reads)});
                return;
            }
            case Stmt::Kind::If:
                collectGlobalReads(stmt->expr.get(), roots);
                collectGlobalStoreDependencies(stmt->thenStmt.get(), roots,
                                               stores);
                collectGlobalStoreDependencies(stmt->elseStmt.get(), roots,
                                               stores);
                return;
            case Stmt::Kind::While:
                collectGlobalReads(stmt->expr.get(), roots);
                collectGlobalStoreDependencies(stmt->body.get(), roots, stores);
                return;
        }
    }

    void markDeadGlobalStores() {
        unordered_map<string, Function *> functions;
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Func) {
                functions[item.func->name] = item.func.get();
            }
        }
        unordered_set<string> reachable;
        vector<string> pending;
        if (functions.count("main")) {
            reachable.insert("main");
            pending.push_back("main");
        }
        for (size_t next = 0; next < pending.size(); ++next) {
            Function *function = functions.at(pending[next]);
            unordered_set<string> called;
            collectCalledNames(function->body.get(), called);
            for (const string &name : called) {
                if (functions.count(name) && reachable.insert(name).second) {
                    pending.push_back(name);
                }
            }
        }

        unordered_set<int> liveGlobals;
        vector<GlobalStoreDependency> stores;
        for (const string &name : reachable) {
            collectGlobalStoreDependencies(functions.at(name)->body.get(),
                                           liveGlobals, stores);
        }
        bool grew = true;
        while (grew) {
            grew = false;
            for (const GlobalStoreDependency &store : stores) {
                if (!liveGlobals.count(store.target)) continue;
                size_t before = liveGlobals.size();
                liveGlobals.insert(store.reads.begin(), store.reads.end());
                grew = grew || liveGlobals.size() != before;
            }
        }
        for (GlobalStoreDependency &store : stores) {
            if (!liveGlobals.count(store.target)) {
                store.stmt->fastDeadStore = true;
            }
        }
    }
};

class RangeAnalyzer {
public:
    explicit RangeAnalyzer(Program &program) : prog(program) {}

    void run() {
        for (auto &item : prog.items) {
            if (item.kind != TopItem::Kind::Func || item.func->fastLocalCount < 0) continue;
            Env env(static_cast<size_t>(item.func->fastLocalCount), full());
            (void)analyzeFlow(item.func->body.get(), env);
        }
    }

private:
    struct Range {
        int64_t lo;
        int64_t hi;
    };
    using Env = vector<Range>;
    struct Flow {
        optional<Env> normal;
        optional<Env> breaks;
        optional<Env> continues;
    };

    Program &prog;
    static constexpr int64_t kMin = numeric_limits<int32_t>::min();
    static constexpr int64_t kMax = numeric_limits<int32_t>::max();

    static Range full() { return {kMin, kMax}; }
    static Range exact(int64_t value) { return {value, value}; }
    static bool same(Range a, Range b) { return a.lo == b.lo && a.hi == b.hi; }
    static Range join(Range a, Range b) {
        return {min(a.lo, b.lo), max(a.hi, b.hi)};
    }

    static bool sameEnv(const Env &a, const Env &b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (!same(a[i], b[i])) return false;
        }
        return true;
    }

    static Env joinEnv(const Env &a, const Env &b) {
        Env out = a;
        for (size_t i = 0; i < out.size(); ++i) out[i] = join(a[i], b[i]);
        return out;
    }

    static Env widenEnv(const Env &oldEnv, const Env &nextEnv) {
        Env out = oldEnv;
        for (size_t i = 0; i < out.size(); ++i) {
            if (nextEnv[i].lo < oldEnv[i].lo) out[i].lo = kMin;
            if (nextEnv[i].hi > oldEnv[i].hi) out[i].hi = kMax;
        }
        return out;
    }

    static bool inInt32(Range r) {
        return r.lo >= kMin && r.hi <= kMax && r.lo <= r.hi;
    }

    static optional<int64_t> exactValue(Range r) {
        if (r.lo == r.hi) return r.lo;
        return nullopt;
    }

    void noteRange(Expr *e, Range r) {
        if (!e) return;
        if (!inInt32(r)) r = full();
        if (!e->rangeAnalyzed) {
            e->rangeAnalyzed = true;
            e->rangeMin = static_cast<int32_t>(r.lo);
            e->rangeMax = static_cast<int32_t>(r.hi);
            return;
        }
        e->rangeMin = min(e->rangeMin, static_cast<int32_t>(r.lo));
        e->rangeMax = max(e->rangeMax, static_cast<int32_t>(r.hi));
    }

    Range analyzeExpr(Expr *e, const Env &env) {
        if (!e) return full();
        Range result = full();
        switch (e->kind) {
            case Expr::Kind::Number:
                result = exact(wrap32(e->value));
                break;
            case Expr::Kind::Var:
                if (!e->fastGlobal && e->fastIndex >= 0 &&
                    e->fastIndex < static_cast<int>(env.size())) {
                    result = env[static_cast<size_t>(e->fastIndex)];
                }
                break;
            case Expr::Kind::Call:
                for (auto &arg : e->args) analyzeExpr(arg.get(), env);
                break;
            case Expr::Kind::Unary: {
                Range value = analyzeExpr(e->lhs.get(), env);
                if (e->op == "+") result = value;
                else if (e->op == "!") result = {0, 1};
                else if (e->op == "-" && value.lo > kMin) {
                    result = {-value.hi, -value.lo};
                }
                break;
            }
            case Expr::Kind::Binary: {
                Range lhs = analyzeExpr(e->lhs.get(), env);
                Range rhs = analyzeExpr(e->rhs.get(), env);
                if (e->op == "<" || e->op == ">" || e->op == "<=" ||
                    e->op == ">=" || e->op == "==" || e->op == "!=" ||
                    e->op == "&&" || e->op == "||") {
                    result = {0, 1};
                    break;
                }
                if (e->op == "+") result = {lhs.lo + rhs.lo, lhs.hi + rhs.hi};
                else if (e->op == "-") result = {lhs.lo - rhs.hi, lhs.hi - rhs.lo};
                else if (e->op == "*") {
                    array<int64_t, 4> values = {
                        lhs.lo * rhs.lo, lhs.lo * rhs.hi,
                        lhs.hi * rhs.lo, lhs.hi * rhs.hi,
                    };
                    result = {*min_element(values.begin(), values.end()),
                              *max_element(values.begin(), values.end())};
                } else if (auto divisor = exactValue(rhs); divisor && *divisor != 0) {
                    int64_t d = *divisor;
                    if (e->op == "/") {
                        int64_t q1 = lhs.lo / d;
                        int64_t q2 = lhs.hi / d;
                        result = {min(q1, q2), max(q1, q2)};
                    } else if (e->op == "%") {
                        int64_t magnitude = d < 0 ? -d : d;
                        int64_t limit = magnitude - 1;
                        if (lhs.lo >= 0) result = {0, min(lhs.hi, limit)};
                        else if (lhs.hi <= 0) result = {max(lhs.lo, -limit), 0};
                        else result = {max(lhs.lo, -limit), min(lhs.hi, limit)};
                    }
                }
                if (!inInt32(result)) result = full();
                break;
            }
        }
        noteRange(e, result);
        return result;
    }

    static string reversedRelation(const string &op) {
        if (op == "<") return ">";
        if (op == ">") return "<";
        if (op == "<=") return ">=";
        if (op == ">=") return "<=";
        return op;
    }

    static string falseRelation(const string &op) {
        if (op == "<") return ">=";
        if (op == ">") return "<=";
        if (op == "<=") return ">";
        if (op == ">=") return "<";
        if (op == "==") return "!=";
        if (op == "!=") return "==";
        return op;
    }

    bool refineSlot(Env &env, int slot, const string &op, int64_t value) const {
        if (slot < 0 || slot >= static_cast<int>(env.size())) return true;
        Range &r = env[static_cast<size_t>(slot)];
        if (op == "<") r.hi = min(r.hi, value - 1);
        else if (op == "<=") r.hi = min(r.hi, value);
        else if (op == ">") r.lo = max(r.lo, value + 1);
        else if (op == ">=") r.lo = max(r.lo, value);
        else if (op == "==") {
            r.lo = max(r.lo, value);
            r.hi = min(r.hi, value);
        }
        // A != constraint is generally non-convex and cannot be represented by
        // one interval, so it is intentionally left unchanged.
        return r.lo <= r.hi;
    }

    bool constrain(Expr *condition, bool wantTrue, Env &env) const {
        if (!condition) return true;
        if (condition->rangeAnalyzed) {
            if (wantTrue && condition->rangeMin == 0 && condition->rangeMax == 0) return false;
            if (!wantTrue && (condition->rangeMin > 0 || condition->rangeMax < 0)) return false;
        }
        if (condition->kind == Expr::Kind::Unary && condition->op == "!") {
            return constrain(condition->lhs.get(), !wantTrue, env);
        }
        if (condition->kind == Expr::Kind::Binary && condition->op == "&&" && wantTrue) {
            return constrain(condition->lhs.get(), true, env) &&
                   constrain(condition->rhs.get(), true, env);
        }
        if (condition->kind == Expr::Kind::Binary && condition->op == "||" && !wantTrue) {
            return constrain(condition->lhs.get(), false, env) &&
                   constrain(condition->rhs.get(), false, env);
        }
        if (condition->kind != Expr::Kind::Binary) return true;
        static const unordered_set<string> relations = {"<", ">", "<=", ">=", "==", "!="};
        if (!relations.count(condition->op)) return true;

        Expr *var = condition->lhs.get();
        Expr *constant = condition->rhs.get();
        string op = condition->op;
        if (!var || var->kind != Expr::Kind::Var || var->fastGlobal ||
            !constant || !constant->rangeAnalyzed || constant->rangeMin != constant->rangeMax) {
            var = condition->rhs.get();
            constant = condition->lhs.get();
            op = reversedRelation(op);
        }
        if (!var || var->kind != Expr::Kind::Var || var->fastGlobal ||
            !constant || !constant->rangeAnalyzed || constant->rangeMin != constant->rangeMax) {
            return true;
        }
        if (!wantTrue) op = falseRelation(op);
        return refineSlot(env, var->fastIndex, op, constant->rangeMin);
    }

    static optional<Env> mergeState(const optional<Env> &lhs, const optional<Env> &rhs) {
        if (!lhs) return rhs;
        if (!rhs) return lhs;
        return joinEnv(*lhs, *rhs);
    }

    static Flow mergeFlow(const Flow &lhs, const Flow &rhs) {
        return Flow{
            mergeState(lhs.normal, rhs.normal),
            mergeState(lhs.breaks, rhs.breaks),
            mergeState(lhs.continues, rhs.continues),
        };
    }

    Flow analyzeFlow(Stmt *s, Env env) {
        if (!s) return Flow{std::move(env), nullopt, nullopt};
        switch (s->kind) {
            case Stmt::Kind::Block: {
                Flow result{std::move(env), nullopt, nullopt};
                for (auto &child : s->stmts) {
                    if (!result.normal) break;
                    Flow next = analyzeFlow(child.get(), std::move(*result.normal));
                    result.normal = std::move(next.normal);
                    result.breaks = mergeState(result.breaks, next.breaks);
                    result.continues = mergeState(result.continues, next.continues);
                }
                return result;
            }
            case Stmt::Kind::Empty:
                return Flow{std::move(env), nullopt, nullopt};
            case Stmt::Kind::Break:
                return Flow{nullopt, std::move(env), nullopt};
            case Stmt::Kind::Continue:
                return Flow{nullopt, nullopt, std::move(env)};
            case Stmt::Kind::ExprStmt:
                analyzeExpr(s->expr.get(), env);
                return Flow{std::move(env), nullopt, nullopt};
            case Stmt::Kind::Return:
                analyzeExpr(s->expr.get(), env);
                return Flow{};
            case Stmt::Kind::DeclStmt: {
                Range value = analyzeExpr(s->decl->init.get(), env);
                if (!s->fastDeadStore && s->decl->fastSlot >= 0 &&
                    s->decl->fastSlot < static_cast<int>(env.size())) {
                    env[static_cast<size_t>(s->decl->fastSlot)] = value;
                }
                return Flow{std::move(env), nullopt, nullopt};
            }
            case Stmt::Kind::Assign: {
                Range value = analyzeExpr(s->expr.get(), env);
                if (!s->fastDeadStore && !s->fastAssignGlobal && s->fastAssignIndex >= 0 &&
                    s->fastAssignIndex < static_cast<int>(env.size())) {
                    env[static_cast<size_t>(s->fastAssignIndex)] = value;
                }
                return Flow{std::move(env), nullopt, nullopt};
            }
            case Stmt::Kind::If: {
                analyzeExpr(s->expr.get(), env);
                Env thenEnv = env;
                Env elseEnv = env;
                bool thenReachable = constrain(s->expr.get(), true, thenEnv);
                bool elseReachable = constrain(s->expr.get(), false, elseEnv);
                Flow thenFlow;
                Flow elseFlow;
                if (thenReachable) thenFlow = analyzeFlow(s->thenStmt.get(), std::move(thenEnv));
                if (elseReachable) elseFlow = analyzeFlow(s->elseStmt.get(), std::move(elseEnv));
                return mergeFlow(thenFlow, elseFlow);
            }
            case Stmt::Kind::While: {
                Env entry = env;
                Env head = entry;
                for (int iteration = 0; iteration < 10; ++iteration) {
                    analyzeExpr(s->expr.get(), head);
                    Env bodyEnv = head;
                    if (!constrain(s->expr.get(), true, bodyEnv)) break;
                    Flow bodyFlow = analyzeFlow(s->body.get(), std::move(bodyEnv));
                    optional<Env> back = mergeState(bodyFlow.normal, bodyFlow.continues);
                    Env next = back ? joinEnv(entry, *back) : entry;
                    if (iteration >= 6) next = widenEnv(head, next);
                    if (sameEnv(head, next)) {
                        head = std::move(next);
                        break;
                    }
                    head = std::move(next);
                }
                analyzeExpr(s->expr.get(), head);
                optional<Env> exitState;
                Env conditionExit = head;
                if (constrain(s->expr.get(), false, conditionExit)) {
                    exitState = std::move(conditionExit);
                }

                Env finalBodyEnv = head;
                if (constrain(s->expr.get(), true, finalBodyEnv)) {
                    Flow finalBody = analyzeFlow(s->body.get(), std::move(finalBodyEnv));
                    exitState = mergeState(exitState, finalBody.breaks);
                }
                return Flow{std::move(exitState), nullopt, nullopt};
            }
        }
        return Flow{std::move(env), nullopt, nullopt};
    }
};

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

static bool exprHasDivMod(const Expr *e) {
    if (!e) return false;
    if (e->kind == Expr::Kind::Binary && (e->op == "/" || e->op == "%")) return true;
    if (exprHasDivMod(e->lhs.get()) || exprHasDivMod(e->rhs.get())) return true;
    for (auto &arg : e->args) {
        if (exprHasDivMod(arg.get())) return true;
    }
    return false;
}

static bool exprUsesOnlyVars(const Expr *e, const unordered_set<string> &allowed) {
    if (!e) return true;
    if (e->kind == Expr::Kind::Var && !allowed.count(e->name)) return false;
    if (!exprUsesOnlyVars(e->lhs.get(), allowed) || !exprUsesOnlyVars(e->rhs.get(), allowed)) return false;
    for (auto &arg : e->args) {
        if (!exprUsesOnlyVars(arg.get(), allowed)) return false;
    }
    return true;
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
        collectProgramNames();
        // Recompute reachability and writes after each rewrite pass.  A first
        // pass can delete a constant-false call or global store, allowing the
        // next pass to prove additional globals immutable from main.
        for (int programRound = 0; programRound < 3; ++programRound) {
            collectGlobalAssignments();
            globalConsts.clear();
            env.clear();
            for (auto &item : prog.items) {
                if (item.kind != TopItem::Kind::Decl) continue;
                optExpr(item.decl->init);
                if (item.decl->isConst ||
                    !assignedGlobalNames.count(item.decl->name)) {
                    if (auto value = foldConstExpr(item.decl->init.get())) {
                        globalConsts[item.decl->name] = *value;
                    }
                }
            }
            for (auto &item : prog.items) {
                if (item.kind != TopItem::Kind::Func) continue;
                env.clear();
                enter();
                for (const string &param : item.func->params) {
                    env.back()[param] = LocalInfo{};
                }
                optStmt(item.func->body);
                leave();
                cseFunction(*item.func);
                dceFunction(*item.func);
            }
        }
    }

private:
    Program &prog;
    unordered_map<string, int32_t> globalConsts;
    unordered_map<string, Function *> inlineableFuncs;
    unordered_map<string, Function *> functions;
    unordered_set<string> globalNames;
    unordered_set<string> assignedGlobalNames;
    struct LocalInfo {
        optional<int32_t> constVal;
        string copyOf;  // empty = not a copy of another local
    };
    vector<unordered_map<string, LocalInfo>> env;

    void collectInlineableFunctions() {
        inlineableFuncs.clear();
        for (auto &item : prog.items) {
            if (item.kind != TopItem::Kind::Func || item.func->returnsVoid) continue;
            const Stmt *body = item.func->body.get();
            if (!body || body->kind != Stmt::Kind::Block || body->stmts.size() != 1) continue;
            const Stmt *ret = body->stmts[0].get();
            if (ret->kind != Stmt::Kind::Return || !ret->expr || exprHasCall(ret->expr.get())) continue;
            unordered_set<string> params(item.func->params.begin(), item.func->params.end());
            if (!exprUsesOnlyVars(ret->expr.get(), params)) continue;
            inlineableFuncs[item.func->name] = item.func.get();
        }
    }

    void collectProgramNames() {
        functions.clear();
        globalNames.clear();
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Decl) globalNames.insert(item.decl->name);
            else functions[item.func->name] = item.func.get();
        }
    }

    static void collectCalledFunctions(const Expr *expr,
                                       unordered_set<string> &called) {
        if (!expr) return;
        if (expr->kind == Expr::Kind::Call) called.insert(expr->name);
        collectCalledFunctions(expr->lhs.get(), called);
        collectCalledFunctions(expr->rhs.get(), called);
        for (auto &arg : expr->args) collectCalledFunctions(arg.get(), called);
    }

    static void collectCalledFunctions(const Stmt *stmt,
                                       unordered_set<string> &called) {
        if (!stmt) return;
        collectCalledFunctions(stmt->expr.get(), called);
        if (stmt->decl) collectCalledFunctions(stmt->decl->init.get(), called);
        for (auto &child : stmt->stmts) {
            collectCalledFunctions(child.get(), called);
        }
        collectCalledFunctions(stmt->thenStmt.get(), called);
        collectCalledFunctions(stmt->elseStmt.get(), called);
        collectCalledFunctions(stmt->body.get(), called);
    }

    static bool scopeContains(
        const vector<unordered_set<string>> &scopes, const string &name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->count(name)) return true;
        }
        return false;
    }

    void collectGlobalAssignments(
        const Stmt *stmt, vector<unordered_set<string>> &scopes) {
        if (!stmt) return;
        switch (stmt->kind) {
            case Stmt::Kind::Block:
                scopes.push_back({});
                for (auto &child : stmt->stmts) {
                    collectGlobalAssignments(child.get(), scopes);
                }
                scopes.pop_back();
                return;
            case Stmt::Kind::DeclStmt:
                if (stmt->decl) scopes.back().insert(stmt->decl->name);
                return;
            case Stmt::Kind::Assign:
                if (!scopeContains(scopes, stmt->name) &&
                    globalNames.count(stmt->name)) {
                    assignedGlobalNames.insert(stmt->name);
                }
                return;
            case Stmt::Kind::If:
                collectGlobalAssignments(stmt->thenStmt.get(), scopes);
                collectGlobalAssignments(stmt->elseStmt.get(), scopes);
                return;
            case Stmt::Kind::While:
                collectGlobalAssignments(stmt->body.get(), scopes);
                return;
            case Stmt::Kind::Empty:
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
            case Stmt::Kind::Return:
                return;
        }
    }

    void collectGlobalAssignments() {
        assignedGlobalNames.clear();
        unordered_set<string> reachable;
        vector<string> pending;
        if (functions.count("main")) {
            reachable.insert("main");
            pending.push_back("main");
        }
        for (size_t next = 0; next < pending.size(); ++next) {
            Function *function = functions.at(pending[next]);
            unordered_set<string> called;
            collectCalledFunctions(function->body.get(), called);
            for (const string &name : called) {
                if (functions.count(name) && reachable.insert(name).second) {
                    pending.push_back(name);
                }
            }
        }
        for (const string &name : reachable) {
            Function *function = functions.at(name);
            vector<unordered_set<string>> scopes(1);
            for (const string &param : function->params) {
                scopes.back().insert(param);
            }
            collectGlobalAssignments(function->body.get(), scopes);
        }
    }

    void enter() { env.push_back({}); }
    void leave() { env.pop_back(); }

    optional<int32_t> lookupLocalConst(const string &name) const {
        for (auto it = env.rbegin(); it != env.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second.constVal;
        }
        return nullopt;
    }

    // Innermost scope entry for a name, or nullptr.
    LocalInfo *findLocal(const string &name) {
        for (auto it = env.rbegin(); it != env.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return &found->second;
        }
        return nullptr;
    }

    // Scope depth of a name's innermost binding (-1 if unknown).
    int localScopeIndex(const string &name) const {
        for (int i = static_cast<int>(env.size()) - 1; i >= 0; --i) {
            if (env[static_cast<size_t>(i)].count(name)) return i;
        }
        return -1;
    }

    bool isKnownLocal(const string &name) const {
        return localScopeIndex(name) >= 0;
    }

    // Any recorded "x is a copy of name" relations become invalid once name
    // is reassigned.
    void killCopiesOf(const string &name) {
        for (auto &scope : env) {
            for (auto &[key, info] : scope) {
                if (info.copyOf == name) info.copyOf.clear();
            }
        }
    }

    void setLocalValue(const string &name, optional<int32_t> value) {
        killCopiesOf(name);
        if (LocalInfo *info = findLocal(name)) {
            info->constVal = value;
            info->copyOf.clear();
        }
    }

    void eraseLocalValue(const string &name) {
        killCopiesOf(name);
        for (auto &scope : env) {
            auto found = scope.find(name);
            if (found != scope.end()) found->second = LocalInfo{};
        }
    }

    // Record "dst reads the same value as the local variable src". Only
    // sources bound at the same or an outer scope stay alive for as long as
    // the relation can be used, so deeper-scoped sources are rejected.
    void recordCopy(const string &dst, const Expr *rhs) {
        if (!rhs || rhs->kind != Expr::Kind::Var || rhs->name == dst) return;
        string src = rhs->name;
        if (LocalInfo *srcInfo = findLocal(src)) {
            if (!srcInfo->copyOf.empty()) src = srcInfo->copyOf;  // collapse chains
        } else {
            return;  // not a local
        }
        int dstIdx = localScopeIndex(dst);
        int srcIdx = localScopeIndex(src);
        if (dstIdx < 0 || srcIdx < 0 || srcIdx > dstIdx || src == dst) return;
        if (LocalInfo *dstInfo = findLocal(dst)) dstInfo->copyOf = src;
    }

    void optExpr(unique_ptr<Expr> &e) {
        if (!e) return;
        switch (e->kind) {
            case Expr::Kind::Number:
                return;
            case Expr::Kind::Var: {
                if (LocalInfo *info = findLocal(e->name)) {
                    if (info->constVal) e = makeNumberExpr(*info->constVal);
                    else if (!info->copyOf.empty()) e->name = info->copyOf;  // copy propagation
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

    // Structural equality of pure (call-free) expressions.
    static bool exprStructEq(const Expr *a, const Expr *b) {
        if (!a || !b) return a == b;
        if (a->kind != b->kind) return false;
        switch (a->kind) {
            case Expr::Kind::Number:
                return a->value == b->value;
            case Expr::Kind::Var:
                return a->name == b->name;
            case Expr::Kind::Unary:
                return a->op == b->op && exprStructEq(a->lhs.get(), b->lhs.get());
            case Expr::Kind::Binary:
                return a->op == b->op && exprStructEq(a->lhs.get(), b->lhs.get()) &&
                       exprStructEq(a->rhs.get(), b->rhs.get());
            case Expr::Kind::Call:
                return false;  // calls are never treated as equal
        }
        return false;
    }

    static unique_ptr<Expr> makeBinary(const string &op, unique_ptr<Expr> lhs, unique_ptr<Expr> rhs) {
        auto e = make_unique<Expr>();
        e->kind = Expr::Kind::Binary;
        e->op = op;
        e->lhs = std::move(lhs);
        e->rhs = std::move(rhs);
        return e;
    }

    void simplifyBinary(unique_ptr<Expr> &e) {
        if (auto v = foldConstExpr(e.get())) {
            e = makeNumberExpr(*v);
            return;
        }
        if (!e || e->kind != Expr::Kind::Binary) return;

        // Canonicalize: constant operand of a commutative operator moves to
        // the right, where both the reassociation rules below and the code
        // generator's immediate forms expect it.
        if ((e->op == "+" || e->op == "*") && foldConstExpr(e->lhs.get()) &&
            !foldConstExpr(e->rhs.get())) {
            swap(e->lhs, e->rhs);
        }

        auto l = foldConstExpr(e->lhs.get());
        auto r = foldConstExpr(e->rhs.get());
        bool lhsPure = !exprHasCall(e->lhs.get());
        bool rhsPure = !exprHasCall(e->rhs.get());

        // Reassociate constant chains: (x op c1) op c2 -> x op (c1 op c2)
        // for +/- mixes and multiplication (32-bit wrapping keeps this exact).
        if (r && e->lhs->kind == Expr::Kind::Binary) {
            Expr *inner = e->lhs.get();
            if (auto ic = foldConstExpr(inner->rhs.get())) {
                long long c1 = *ic, c2 = *r;
                if ((e->op == "+" || e->op == "-") && (inner->op == "+" || inner->op == "-")) {
                    long long outer = e->op == "+" ? c2 : -c2;
                    long long innerC = inner->op == "+" ? c1 : -c1;
                    long long total = wrap32(innerC + outer);
                    e = makeBinary("+", std::move(inner->lhs), makeNumberExpr(total));
                    simplifyBinary(e);
                    return;
                }
                if (e->op == "*" && inner->op == "*") {
                    e = makeBinary("*", std::move(inner->lhs), makeNumberExpr(wrap32(c1 * c2)));
                    simplifyBinary(e);
                    return;
                }
            }
        }

        if (e->op == "+" && r && *r == 0) e = std::move(e->lhs);
        else if (e->op == "+" && l && *l == 0) e = std::move(e->rhs);
        else if (e->op == "-" && r && *r == 0) e = std::move(e->lhs);
        else if (e->op == "-" && lhsPure && exprStructEq(e->lhs.get(), e->rhs.get())) e = makeNumberExpr(0);
        else if (e->op == "*" && r && *r == 1) e = std::move(e->lhs);
        else if (e->op == "*" && l && *l == 1) e = std::move(e->rhs);
        else if (e->op == "*" && r && *r == 0 && lhsPure) e = makeNumberExpr(0);
        else if (e->op == "*" && l && *l == 0 && rhsPure) e = makeNumberExpr(0);
        else if (e->op == "/" && r && *r == 1) e = std::move(e->lhs);
        else if (e->op == "%" && r && *r == 1 && lhsPure) e = makeNumberExpr(0);
        else if (e->op == "+" && lhsPure && exprStructEq(e->lhs.get(), e->rhs.get())) {
            e = makeBinary("*", std::move(e->lhs), makeNumberExpr(2));
        } else if (e->op == "&&" && l && !truthy(*l)) e = makeNumberExpr(0);
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
                killCopiesOf(s->decl->name);  // shadowed outer relations die here
                env.back()[s->decl->name] = LocalInfo{foldConstExpr(s->decl->init.get()), ""};
                recordCopy(s->decl->name, s->decl->init.get());
                break;
            case Stmt::Kind::Assign:
                optExpr(s->expr);
                if (isKnownLocal(s->name)) {
                    setLocalValue(s->name, foldConstExpr(s->expr.get()));
                    recordCopy(s->name, s->expr.get());
                }
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
                if (hoistLoopInvariants(s)) {
                    // s is now a Block{temp decls..., while}; reprocess it.
                    optStmt(s);
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

    // ---- backward-liveness dead code elimination ----
    //
    // Removes assignments to locals that are never read afterwards, pure
    // expression statements, and control flow that only guards dead code
    // (an if whose branches empty out no longer keeps its condition's
    // operands alive). Globals are conservatively always live here; the
    // slot-based liveness analysis handles them later. Loop bodies are
    // analyzed to a fixpoint without mutation first, then rewritten once
    // under the converged (conservative) live set.

    struct DceCtx {
        const unordered_set<string> *breakLive = nullptr;
        const unordered_set<string> *continueLive = nullptr;
    };

    // Names referenced (read or written) by statements the pass keeps. A
    // declaration may only be deleted when its name never appears afterwards:
    // liveness alone is not enough, because a kept later store kills liveness
    // backwards yet still needs the symbol to exist.
    unordered_set<string> dceUsed;

    static void addReads(const Expr *e, unordered_set<string> &live) {
        if (!e) return;
        if (e->kind == Expr::Kind::Var) {
            live.insert(e->name);
            return;
        }
        addReads(e->lhs.get(), live);
        addReads(e->rhs.get(), live);
        for (auto &arg : e->args) addReads(arg.get(), live);
    }

    void dceReads(const Expr *e, unordered_set<string> &live) {
        addReads(e, live);
        addReads(e, dceUsed);
    }

    // Returns true when the statement is (or, after mutation, became) free of
    // live effects and thus deletable. In analyze-only mode nothing is
    // rewritten, but the return value still reflects what mutation would do,
    // so a loop fixpoint sees the same live sets the rewrite will use.
    bool dceStmt(unique_ptr<Stmt> &s, unordered_set<string> &live, DceCtx ctx, bool mutate) {
        if (!s) return true;
        switch (s->kind) {
            case Stmt::Kind::Block: {
                bool allDead = true;
                for (auto it = s->stmts.rbegin(); it != s->stmts.rend(); ++it) {
                    if (!dceStmt(*it, live, ctx, mutate)) allDead = false;
                }
                if (mutate) {
                    s->stmts.erase(remove_if(s->stmts.begin(), s->stmts.end(),
                                             [](const unique_ptr<Stmt> &c) {
                                                 return !c || c->kind == Stmt::Kind::Empty;
                                             }),
                                   s->stmts.end());
                }
                return allDead;
            }
            case Stmt::Kind::Empty:
                return true;
            case Stmt::Kind::ExprStmt:
                if (!exprHasCall(s->expr.get())) {
                    if (mutate) s = emptyStmt();
                    return true;
                }
                dceReads(s->expr.get(), live);
                return false;
            case Stmt::Kind::Assign: {
                bool isGlobalName = globalNames.count(s->name) != 0;
                if (!isGlobalName && !live.count(s->name)) {
                    if (!exprHasCall(s->expr.get())) {
                        if (mutate) s = emptyStmt();
                        return true;
                    }
                    if (mutate) {
                        s->kind = Stmt::Kind::ExprStmt;  // keep the call, drop the store
                        s->name.clear();
                    }
                    dceReads(s->expr.get(), live);
                    return false;
                }
                if (!isGlobalName) live.erase(s->name);
                dceUsed.insert(s->name);  // the kept store still needs the symbol
                dceReads(s->expr.get(), live);
                return false;
            }
            case Stmt::Kind::DeclStmt: {
                const string &name = s->decl->name;
                if (!live.count(name) && !dceUsed.count(name)) {
                    if (!s->decl->init || !exprHasCall(s->decl->init.get())) {
                        if (mutate) s = emptyStmt();
                        return true;
                    }
                    if (mutate) {
                        auto e = std::move(s->decl->init);
                        s->kind = Stmt::Kind::ExprStmt;
                        s->expr = std::move(e);
                        s->decl.reset();
                        dceReads(s->expr.get(), live);
                        return false;
                    }
                    dceReads(s->decl->init.get(), live);
                    return false;
                }
                live.erase(name);
                if (s->decl->init) dceReads(s->decl->init.get(), live);
                return false;
            }
            case Stmt::Kind::If: {
                auto liveThen = live;
                bool thenDead = dceStmt(s->thenStmt, liveThen, ctx, mutate);
                auto liveElse = live;
                bool elseDead = dceStmt(s->elseStmt, liveElse, ctx, mutate);
                if (thenDead && elseDead && !exprHasCall(s->expr.get())) {
                    if (mutate) s = emptyStmt();
                    return true;  // condition operands stay dead
                }
                live = liveThen;
                live.insert(liveElse.begin(), liveElse.end());
                dceReads(s->expr.get(), live);
                return false;
            }
            case Stmt::Kind::While: {
                unordered_set<string> exitLive = live;
                unordered_set<string> head = live;
                dceReads(s->expr.get(), head);
                // Analyze-only fixpoint: converge the loop-head live set.
                for (int iter = 0; iter < 64; ++iter) {
                    auto trial = head;
                    DceCtx inner{&exitLive, &head};
                    dceStmt(s->body, trial, inner, false);
                    size_t before = head.size();
                    head.insert(trial.begin(), trial.end());
                    if (head.size() == before) break;
                }
                if (mutate) {
                    auto bodyLive = head;
                    DceCtx inner{&exitLive, &head};
                    dceStmt(s->body, bodyLive, inner, true);
                }
                live = head;
                return false;  // whole-loop deletion is left to later passes
            }
            case Stmt::Kind::Break:
                if (ctx.breakLive) live = *ctx.breakLive;
                return false;
            case Stmt::Kind::Continue:
                if (ctx.continueLive) live = *ctx.continueLive;
                return false;
            case Stmt::Kind::Return:
                live.clear();
                dceReads(s->expr.get(), live);
                return false;
        }
        return false;
    }

    bool collectUniqueDceNames(const Stmt *s, unordered_set<string> &names) const {
        if (!s) return true;
        if (s->kind == Stmt::Kind::DeclStmt && s->decl &&
            !names.insert(s->decl->name).second) {
            return false;
        }
        for (auto &child : s->stmts) {
            if (!collectUniqueDceNames(child.get(), names)) return false;
        }
        return collectUniqueDceNames(s->thenStmt.get(), names) &&
               collectUniqueDceNames(s->elseStmt.get(), names) &&
               collectUniqueDceNames(s->body.get(), names);
    }

    void dceFunction(Function &f) {
        unordered_set<string> names;
        for (const string &param : f.params) {
            if (!names.insert(param).second) return;
        }
        // This DCE tracks liveness by source name. If a function contains
        // shadowed or reused local names, a declaration in an inner/disjoint
        // scope could otherwise kill the live bit for a different binding.
        if (!collectUniqueDceNames(f.body.get(), names)) return;
        dceUsed.clear();
        unordered_set<string> live;
        dceStmt(f.body, live, DceCtx{}, true);
    }

    // ---- local common subexpression elimination ----
    //
    // Within a block's statement sequence, repeated pure subexpressions are
    // computed once in a temporary declared at the first occurrence. Only
    // maximal candidate subtrees are recorded per expression (recorded and
    // matched nodes are disjoint, so no slot pointer can dangle); running
    // multiple optimizer rounds lets smaller shared pieces surface later,
    // with copy propagation and DCE cleaning up temp-to-temp chains.
    // Availability is invalidated by writes to any operand and, for
    // global-reading expressions, by any call. Branch- and loop-internal
    // scans start fresh, so a temp's defining occurrence is always at least
    // as unconditional as the occurrences it replaces.

    int cseCounter = 0;

    struct CseEntry {
        unique_ptr<Expr> *slot;
        int idx;
        string temp;
        unordered_set<string> reads;
        bool readsGlobal = false;
    };

    struct CseInsert {
        int idx;
        unique_ptr<Stmt> decl;
    };

    bool cseCandidate(const Expr *e) const {
        if (e->kind != Expr::Kind::Binary && e->kind != Expr::Kind::Unary) return false;
        if (exprHasCall(e)) return false;
        if (foldConstExpr(e)) return false;
        int ops = 0;
        bool hasMulDiv = false;
        licmCost(e, ops, hasMulDiv);
        return hasMulDiv || ops >= 2;
    }

    static unique_ptr<Expr> makeVarExpr(const string &name) {
        auto v = make_unique<Expr>();
        v->kind = Expr::Kind::Var;
        v->name = name;
        return v;
    }

    static bool exprContainsSlot(const Expr *root, const unique_ptr<Expr> *slot) {
        if (!root) return false;
        if (&root->lhs == slot || &root->rhs == slot) return true;
        for (auto &arg : root->args) {
            if (&arg == slot) return true;
        }
        if (exprContainsSlot(root->lhs.get(), slot) || exprContainsSlot(root->rhs.get(), slot)) return true;
        for (auto &arg : root->args) {
            if (exprContainsSlot(arg.get(), slot)) return true;
        }
        return false;
    }

    bool cseReadsGlobal(const Expr *e) const {
        unordered_set<string> reads;
        addReads(e, reads);
        for (const string &name : reads) {
            if (globalNames.count(name)) return true;
        }
        return false;
    }

    // noGlobals: the surrounding expression contains a call, so global values
    // may change mid-expression; global-reading subexpressions must neither
    // match existing entries nor be recorded.
    void cseVisitExpr(unique_ptr<Expr> &e, int idx, vector<CseEntry> &avail,
                      vector<CseInsert> &inserts, bool noGlobals = false) {
        if (!e) return;
        if (cseCandidate(e.get()) && !(noGlobals && cseReadsGlobal(e.get()))) {
            for (auto &entry : avail) {
                if (!exprStructEq(*entry.slot ? entry.slot->get() : nullptr, e.get())) continue;
                if (entry.temp.empty()) {
                    entry.temp = "$cse" + to_string(cseCounter++);
                    auto decl = make_unique<Stmt>();
                    decl->kind = Stmt::Kind::DeclStmt;
                    decl->decl = make_unique<Decl>();
                    decl->decl->name = entry.temp;
                    decl->decl->init = std::move(*entry.slot);
                    *entry.slot = makeVarExpr(entry.temp);
                    // If the first occurrence now lives inside an already
                    // materialized temp's initializer, this temp must be
                    // declared before that one.
                    size_t pos = inserts.size();
                    for (size_t k = 0; k < inserts.size(); ++k) {
                        if (inserts[k].decl &&
                            exprContainsSlot(inserts[k].decl->decl->init.get(), entry.slot)) {
                            pos = k;
                            break;
                        }
                    }
                    CseInsert ins{pos < inserts.size() ? inserts[pos].idx : entry.idx, std::move(decl)};
                    entry.slot = &ins.decl->decl->init;
                    inserts.insert(inserts.begin() + static_cast<long>(pos), std::move(ins));
                }
                e = makeVarExpr(entry.temp);
                return;
            }
            CseEntry entry;
            entry.slot = &e;
            entry.idx = idx;
            addReads(e.get(), entry.reads);
            for (const string &name : entry.reads) {
                if (globalNames.count(name)) {
                    entry.readsGlobal = true;
                    break;
                }
            }
            avail.push_back(std::move(entry));
            // fall through: sub-candidates are recorded too, so a later
            // repeat of an inner piece can still be shared
        }
        cseVisitExpr(e->lhs, idx, avail, inserts, noGlobals);
        cseVisitExpr(e->rhs, idx, avail, inserts, noGlobals);
        for (auto &arg : e->args) cseVisitExpr(arg, idx, avail, inserts, noGlobals);
    }

    static void cseInvalidate(vector<CseEntry> &avail, const unordered_set<string> &written) {
        avail.erase(remove_if(avail.begin(), avail.end(),
                              [&](const CseEntry &entry) {
                                  for (const string &name : written) {
                                      if (entry.reads.count(name)) return true;
                                  }
                                  return false;
                              }),
                    avail.end());
    }

    static void cseDropGlobals(vector<CseEntry> &avail) {
        avail.erase(remove_if(avail.begin(), avail.end(),
                              [](const CseEntry &entry) { return entry.readsGlobal; }),
                    avail.end());
    }

    void cseNested(Stmt *s) {
        if (!s) return;
        if (s->kind == Stmt::Kind::Block) {
            cseBlock(s->stmts);
            return;
        }
        // Single-statement branch bodies: recurse into any nested structure.
        cseNested(s->thenStmt.get());
        cseNested(s->elseStmt.get());
        cseNested(s->body.get());
    }

    void cseBlock(vector<unique_ptr<Stmt>> &stmts) {
        vector<CseEntry> avail;
        vector<CseInsert> inserts;
        for (int i = 0; i < static_cast<int>(stmts.size()); ++i) {
            Stmt *s = stmts[static_cast<size_t>(i)].get();
            switch (s->kind) {
                case Stmt::Kind::ExprStmt:
                case Stmt::Kind::Return: {
                    bool call = s->expr && exprHasCall(s->expr.get());
                    if (call) cseDropGlobals(avail);
                    cseVisitExpr(s->expr, i, avail, inserts, call);
                    if (call) cseDropGlobals(avail);
                    break;
                }
                case Stmt::Kind::Assign: {
                    bool call = exprHasCall(s->expr.get());
                    if (call) cseDropGlobals(avail);
                    cseVisitExpr(s->expr, i, avail, inserts, call);
                    if (call) cseDropGlobals(avail);
                    cseInvalidate(avail, {s->name});
                    break;
                }
                case Stmt::Kind::DeclStmt:
                    if (s->decl->init) {
                        bool call = exprHasCall(s->decl->init.get());
                        if (call) cseDropGlobals(avail);
                        cseVisitExpr(s->decl->init, i, avail, inserts, call);
                        if (call) cseDropGlobals(avail);
                    }
                    cseInvalidate(avail, {s->decl->name});
                    break;
                case Stmt::Kind::If: {
                    bool condCall = exprHasCall(s->expr.get());
                    if (condCall) cseDropGlobals(avail);
                    cseVisitExpr(s->expr, i, avail, inserts, condCall);
                    cseNested(s->thenStmt.get());
                    cseNested(s->elseStmt.get());
                    cseInvalidate(avail, assignedInStmt(s));
                    if (stmtHasCall(s)) cseDropGlobals(avail);
                    break;
                }
                case Stmt::Kind::While: {
                    cseInvalidate(avail, assignedInStmt(s->body.get()));
                    if (stmtHasCall(s->body.get()) || exprHasCall(s->expr.get())) cseDropGlobals(avail);
                    cseNested(s->body.get());
                    break;
                }
                case Stmt::Kind::Block:
                    cseBlock(s->stmts);
                    cseInvalidate(avail, assignedInStmt(s));
                    if (stmtHasCall(s)) cseDropGlobals(avail);
                    break;
                default:
                    break;
            }
        }
        if (inserts.empty()) return;
        vector<unique_ptr<Stmt>> result;
        result.reserve(stmts.size() + inserts.size());
        for (int i = 0; i < static_cast<int>(stmts.size()); ++i) {
            for (auto &ins : inserts) {
                if (ins.idx == i && ins.decl) result.push_back(std::move(ins.decl));
            }
            result.push_back(std::move(stmts[static_cast<size_t>(i)]));
        }
        stmts = std::move(result);
    }

    void cseFunction(Function &f) {
        if (f.body && f.body->kind == Stmt::Kind::Block) cseBlock(f.body->stmts);
    }

    // ---- loop-invariant code motion ----
    //
    // Pure subexpressions whose operands cannot change during the loop are
    // computed once in temporaries declared just before the loop. Divisions
    // are hoisted only for safe constant divisors, so a division guarded
    // inside the loop can never change behavior by being evaluated early.

    int licmCounter = 0;

    struct LicmCtx {
        unordered_set<string> modified;   // names assigned/declared in the loop
        bool callsInBody = false;         // calls may write any global
        vector<pair<unique_ptr<Expr>, string>> hoisted;
    };

    bool stmtHasCall(const Stmt *s) const {
        if (!s) return false;
        if (s->expr && exprHasCall(s->expr.get())) return true;
        if (s->decl && s->decl->init && exprHasCall(s->decl->init.get())) return true;
        for (auto &child : s->stmts) {
            if (stmtHasCall(child.get())) return true;
        }
        return stmtHasCall(s->thenStmt.get()) || stmtHasCall(s->elseStmt.get()) ||
               stmtHasCall(s->body.get());
    }

    bool licmInvariant(const Expr *e, const LicmCtx &ctx) const {
        if (!e) return true;
        switch (e->kind) {
            case Expr::Kind::Number:
                return true;
            case Expr::Kind::Var:
                if (ctx.modified.count(e->name)) return false;
                if (isKnownLocal(e->name)) return true;       // locals are call-proof
                if (!globalNames.count(e->name)) return false;
                return !ctx.callsInBody;                      // globals only without calls
            case Expr::Kind::Unary:
                return licmInvariant(e->lhs.get(), ctx);
            case Expr::Kind::Binary:
                if (e->op == "/" || e->op == "%") {
                    // Only hoist divisions whose divisor is a safe constant;
                    // evaluating a guarded division early must not be able to
                    // change semantics anywhere in the target program.
                    auto d = foldConstExpr(e->rhs.get());
                    if (!d || *d == 0 || *d == -1) return false;
                }
                return licmInvariant(e->lhs.get(), ctx) && licmInvariant(e->rhs.get(), ctx);
            case Expr::Kind::Call:
                return false;
        }
        return false;
    }

    static void licmCost(const Expr *e, int &ops, bool &hasMulDiv) {
        if (!e) return;
        if (e->kind == Expr::Kind::Binary) {
            ++ops;
            if (e->op == "*" || e->op == "/" || e->op == "%") hasMulDiv = true;
        } else if (e->kind == Expr::Kind::Unary) {
            ++ops;
        }
        licmCost(e->lhs.get(), ops, hasMulDiv);
        licmCost(e->rhs.get(), ops, hasMulDiv);
    }

    bool licmWorthHoisting(const Expr *e) const {
        if (e->kind != Expr::Kind::Binary && e->kind != Expr::Kind::Unary) return false;
        if (foldConstExpr(e)) return false;  // plain constants stay as immediates
        int ops = 0;
        bool hasMulDiv = false;
        licmCost(e, ops, hasMulDiv);
        return hasMulDiv || ops >= 2;
    }

    void licmVisitExpr(unique_ptr<Expr> &e, LicmCtx &ctx) {
        if (!e) return;
        if (licmInvariant(e.get(), ctx)) {
            if (!licmWorthHoisting(e.get())) return;
            for (auto &[expr, name] : ctx.hoisted) {
                if (exprStructEq(expr.get(), e.get())) {
                    auto v = make_unique<Expr>();
                    v->kind = Expr::Kind::Var;
                    v->name = name;
                    e = std::move(v);
                    return;
                }
            }
            string name = "$licm" + to_string(licmCounter++);
            auto v = make_unique<Expr>();
            v->kind = Expr::Kind::Var;
            v->name = name;
            ctx.hoisted.push_back({std::move(e), name});
            e = std::move(v);
            return;
        }
        licmVisitExpr(e->lhs, ctx);
        licmVisitExpr(e->rhs, ctx);
        for (auto &arg : e->args) licmVisitExpr(arg, ctx);
    }

    void licmVisitStmt(Stmt *s, LicmCtx &ctx) {
        if (!s) return;
        if (s->expr) licmVisitExpr(s->expr, ctx);
        if (s->decl && s->decl->init) licmVisitExpr(s->decl->init, ctx);
        for (auto &child : s->stmts) licmVisitStmt(child.get(), ctx);
        licmVisitStmt(s->thenStmt.get(), ctx);
        licmVisitStmt(s->elseStmt.get(), ctx);
        licmVisitStmt(s->body.get(), ctx);
    }

    bool hoistLoopInvariants(unique_ptr<Stmt> &s) {
        LicmCtx ctx;
        ctx.modified = assignedInStmt(s->body.get());
        ctx.callsInBody = stmtHasCall(s->body.get());
        licmVisitExpr(s->expr, ctx);
        licmVisitStmt(s->body.get(), ctx);
        if (ctx.hoisted.empty()) return false;
        auto block = make_unique<Stmt>();
        block->kind = Stmt::Kind::Block;
        for (auto &[expr, name] : ctx.hoisted) {
            auto decl = make_unique<Stmt>();
            decl->kind = Stmt::Kind::DeclStmt;
            decl->decl = make_unique<Decl>();
            decl->decl->name = name;
            decl->decl->init = std::move(expr);
            block->stmts.push_back(std::move(decl));
        }
        block->stmts.push_back(std::move(s));
        s = std::move(block);
        return true;
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

// Closed-form optimization for affine and bounded residue-periodic counted loops.
//
// For a loop whose body is a sequence of affine assignments, one iteration is
// a matrix transformation over 32-bit wrapping integers.  If local constant
// data flow proves the initial induction value, invariant bound, monotonic
// step, and exact finite trip count, exponentiating that matrix gives the same
// final state without executing the loop in the compiler.  Branches driven only
// by a proven finite induction-variable residue period are composed one abstract
// period at a time.  The rewritten AST keeps an entry-condition branch and
// computes the final values at runtime.
//
// Loop helpers use the same proof machinery.  The bounded statement walk only
// specializes non-loop control flow for one proven residue phase; every while
// statement must be discharged by a counted-loop proof and matrix
// exponentiation.  No ToyC loop is iterated and no ToyC function is invoked by
// the compiler.
class AffineLoopOptimizer {
public:
    explicit AffineLoopOptimizer(Program &program) : prog(program) {}

    bool run() {
        collectProgramInfo();
        for (auto &item : prog.items) {
            if (item.kind != TopItem::Kind::Func || item.func->fastLocalCount < 0) continue;
            ExactEnv env(static_cast<size_t>(item.func->fastLocalCount));
            optimizeStmt(item.func->body, env);
        }
        return changed;
    }

private:
    static constexpr int kGlobalBit = 1 << 30;
    using ExactEnv = vector<optional<int32_t>>;
    using Row = vector<uint32_t>;
    using Matrix = vector<Row>;
    static constexpr int kPolynomialTerms = 6;

    struct Polynomial {
        array<uint32_t, kPolynomialTerms> coefficients{};
    };

    struct SymbolicAffine {
        unique_ptr<Expr> constant;
        unique_ptr<Expr> linear;
    };

    struct CountedLoop {
        int inductionKey = -1;
        string inductionName;
        string relation;
        const Expr *boundExpr = nullptr;
        int32_t start = 0;
        int32_t bound = 0;
        int32_t step = 0;
        int32_t conditionOffset = 0;
        int32_t finalValue = 0;
        uint64_t trips = 0;
    };

    struct Model {
        vector<int> keys;
        unordered_map<int, int> index;
        unordered_map<int, string> names;
        unordered_set<int> modified;
        unordered_set<int> transient;
    };

    enum class HelperFlow { Fallthrough, Returned, Fail };

    Program &prog;
    unordered_map<int, int32_t> exactGlobals;
    unordered_set<int> constGlobals;
    unordered_map<string, const Function *> branchHelpers;
    unordered_map<string, const Function *> exactLoopHelpers;
    unordered_set<string> usedNames;
    int freshId = 0;
    bool changed = false;

    static int exprKey(const Expr *e) {
        if (!e || e->kind != Expr::Kind::Var || e->fastIndex < 0) return -1;
        return e->fastGlobal ? (kGlobalBit | e->fastIndex) : e->fastIndex;
    }

    static int assignKey(const Stmt *s) {
        if (!s || s->kind != Stmt::Kind::Assign || s->fastAssignIndex < 0) return -1;
        return s->fastAssignGlobal ? (kGlobalBit | s->fastAssignIndex) : s->fastAssignIndex;
    }

    static bool isGlobalKey(int key) {
        return key >= 0 && (key & kGlobalBit) != 0;
    }

    static int globalIndex(int key) {
        return key & ~kGlobalBit;
    }

    static bool branchHelperShape(const Stmt *s,
                                  const unordered_set<string> &params,
                                  int &nodes, bool &alwaysReturns) {
        alwaysReturns = false;
        if (!s || ++nodes > 64) return false;
        switch (s->kind) {
            case Stmt::Kind::Block: {
                bool returned = false;
                for (auto &child : s->stmts) {
                    if (returned) break;
                    bool childReturns = false;
                    if (!branchHelperShape(child.get(), params, nodes, childReturns)) {
                        return false;
                    }
                    returned = childReturns;
                }
                alwaysReturns = returned;
                return true;
            }
            case Stmt::Kind::Empty:
                return true;
            case Stmt::Kind::If: {
                if (!s->expr || exprHasCall(s->expr.get()) ||
                    !exprUsesOnlyVars(s->expr.get(), params)) {
                    return false;
                }
                bool thenReturns = false;
                bool elseReturns = false;
                if (!branchHelperShape(s->thenStmt.get(), params, nodes, thenReturns)) {
                    return false;
                }
                if (s->elseStmt &&
                    !branchHelperShape(s->elseStmt.get(), params, nodes, elseReturns)) {
                    return false;
                }
                alwaysReturns = thenReturns && s->elseStmt && elseReturns;
                return true;
            }
            case Stmt::Kind::Return:
                if (!s->expr || exprHasCall(s->expr.get()) ||
                    !exprUsesOnlyVars(s->expr.get(), params)) {
                    return false;
                }
                alwaysReturns = true;
                return true;
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Assign:
            case Stmt::Kind::DeclStmt:
            case Stmt::Kind::While:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return false;
        }
        return false;
    }

    static bool exactLoopHelperExprShape(const Expr *e, int localCount,
                                         int &nodes) {
        if (!e) return true;
        if (++nodes > 256 || e->kind == Expr::Kind::Call) return false;
        if (e->kind == Expr::Kind::Var) {
            return !e->fastGlobal && e->fastIndex >= 0 &&
                   e->fastIndex < localCount;
        }
        if (!exactLoopHelperExprShape(e->lhs.get(), localCount, nodes) ||
            !exactLoopHelperExprShape(e->rhs.get(), localCount, nodes)) {
            return false;
        }
        for (auto &arg : e->args) {
            if (!exactLoopHelperExprShape(arg.get(), localCount, nodes)) return false;
        }
        return true;
    }

    static bool exactLoopHelperStmtShape(const Stmt *s, int loopDepth,
                                         int localCount, int &nodes,
                                         bool &hasLoop, bool &alwaysReturns) {
        alwaysReturns = false;
        if (!s || ++nodes > 256) return false;
        switch (s->kind) {
            case Stmt::Kind::Block: {
                bool returned = false;
                for (auto &child : s->stmts) {
                    if (returned) break;
                    bool childReturns = false;
                    if (!exactLoopHelperStmtShape(child.get(), loopDepth,
                                                  localCount, nodes, hasLoop,
                                                  childReturns)) {
                        return false;
                    }
                    returned = childReturns;
                }
                alwaysReturns = returned;
                return true;
            }
            case Stmt::Kind::Empty:
                return true;
            case Stmt::Kind::ExprStmt:
                return exactLoopHelperExprShape(s->expr.get(), localCount, nodes);
            case Stmt::Kind::DeclStmt:
                return s->decl && s->decl->init && s->decl->fastSlot >= 0 &&
                       s->decl->fastSlot < localCount &&
                       exactLoopHelperExprShape(s->decl->init.get(), localCount,
                                                nodes);
            case Stmt::Kind::Assign:
                return !s->fastAssignGlobal && s->fastAssignIndex >= 0 &&
                       s->fastAssignIndex < localCount &&
                       exactLoopHelperExprShape(s->expr.get(), localCount, nodes);
            case Stmt::Kind::If: {
                if (!exactLoopHelperExprShape(s->expr.get(), localCount, nodes)) {
                    return false;
                }
                bool thenReturns = false;
                bool elseReturns = false;
                if (!exactLoopHelperStmtShape(s->thenStmt.get(), loopDepth,
                                              localCount, nodes, hasLoop,
                                              thenReturns)) {
                    return false;
                }
                if (s->elseStmt &&
                    !exactLoopHelperStmtShape(s->elseStmt.get(), loopDepth,
                                              localCount, nodes, hasLoop,
                                              elseReturns)) {
                    return false;
                }
                alwaysReturns = thenReturns && s->elseStmt && elseReturns;
                return true;
            }
            case Stmt::Kind::While: {
                if (!exactLoopHelperExprShape(s->expr.get(), localCount, nodes)) {
                    return false;
                }
                hasLoop = true;
                bool bodyReturns = false;
                if (!exactLoopHelperStmtShape(s->body.get(), loopDepth + 1,
                                              localCount, nodes, hasLoop,
                                              bodyReturns) || bodyReturns) {
                    return false;
                }
                return true;
            }
            case Stmt::Kind::Return:
                if (loopDepth != 0 || !s->expr ||
                    !exactLoopHelperExprShape(s->expr.get(), localCount, nodes)) {
                    return false;
                }
                alwaysReturns = true;
                return true;
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return false;
        }
        return false;
    }

    void collectProgramInfo() {
        int global = 0;
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Decl) {
                usedNames.insert(item.decl->name);
                if (item.decl->isConst) {
                    if (auto value = foldConstExpr(item.decl->init.get())) {
                        exactGlobals[global] = *value;
                    }
                    constGlobals.insert(global);
                }
                ++global;
            } else {
                usedNames.insert(item.func->name);
                for (const string &param : item.func->params) usedNames.insert(param);
                collectNames(item.func->body.get());
                if (!item.func->returnsVoid) {
                    unordered_set<string> params(item.func->params.begin(),
                                                 item.func->params.end());
                    int nodes = 0;
                    bool alwaysReturns = false;
                    if (branchHelperShape(item.func->body.get(), params, nodes,
                                          alwaysReturns) && alwaysReturns) {
                        branchHelpers[item.func->name] = item.func.get();
                    }
                }
                if (!item.func->returnsVoid && item.func->fastLocalCount >=
                        static_cast<int>(item.func->params.size()) &&
                    item.func->fastLocalCount <= 32) {
                    int nodes = 0;
                    bool hasLoop = false;
                    bool alwaysReturns = false;
                    if (exactLoopHelperStmtShape(item.func->body.get(), 0,
                                                 item.func->fastLocalCount,
                                                 nodes, hasLoop, alwaysReturns) &&
                        hasLoop && alwaysReturns) {
                        exactLoopHelpers[item.func->name] = item.func.get();
                    }
                }
            }
        }
    }

    void collectNames(const Stmt *s) {
        if (!s) return;
        if (!s->name.empty()) usedNames.insert(s->name);
        if (s->decl) usedNames.insert(s->decl->name);
        for (auto &child : s->stmts) collectNames(child.get());
        collectNames(s->thenStmt.get());
        collectNames(s->elseStmt.get());
        collectNames(s->body.get());
    }

    string freshName() {
        while (true) {
            string name = "__toyc_affine_" + to_string(freshId++);
            if (usedNames.insert(name).second) return name;
        }
    }

    optional<int32_t> evalExact(
        const Expr *e, const ExactEnv &env,
        const vector<const Expr *> *params = nullptr) const {
        if (!e) return nullopt;
        switch (e->kind) {
            case Expr::Kind::Number:
                return wrap32(e->value);
            case Expr::Kind::Var:
                if (params && !e->fastGlobal && e->fastIndex >= 0 &&
                    e->fastIndex < static_cast<int>(params->size())) {
                    return evalExact((*params)[static_cast<size_t>(e->fastIndex)], env);
                }
                if (e->fastGlobal) {
                    auto found = exactGlobals.find(e->fastIndex);
                    return found == exactGlobals.end() ? nullopt
                                                       : optional<int32_t>(found->second);
                }
                if (e->fastIndex < 0 || e->fastIndex >= static_cast<int>(env.size())) return nullopt;
                return env[static_cast<size_t>(e->fastIndex)];
            case Expr::Kind::Call:
                return nullopt;
            case Expr::Kind::Unary: {
                auto value = evalExact(e->lhs.get(), env, params);
                if (!value) return nullopt;
                if (e->op == "+") return *value;
                if (e->op == "-") return sub32(0, *value);
                if (e->op == "!") return !truthy(*value);
                return nullopt;
            }
            case Expr::Kind::Binary: {
                if (e->op == "&&") {
                    auto lhs = evalExact(e->lhs.get(), env, params);
                    if (!lhs) return nullopt;
                    if (!truthy(*lhs)) return 0;
                    auto rhs = evalExact(e->rhs.get(), env, params);
                    return rhs ? optional<int32_t>(truthy(*rhs)) : nullopt;
                }
                if (e->op == "||") {
                    auto lhs = evalExact(e->lhs.get(), env, params);
                    if (!lhs) return nullopt;
                    if (truthy(*lhs)) return 1;
                    auto rhs = evalExact(e->rhs.get(), env, params);
                    return rhs ? optional<int32_t>(truthy(*rhs)) : nullopt;
                }
                auto lhs = evalExact(e->lhs.get(), env, params);
                auto rhs = evalExact(e->rhs.get(), env, params);
                if (!lhs || !rhs) return nullopt;
                if (e->op == "+") return add32(*lhs, *rhs);
                if (e->op == "-") return sub32(*lhs, *rhs);
                if (e->op == "*") return mul32(*lhs, *rhs);
                if (e->op == "/") return *rhs == 0 ? nullopt : optional<int32_t>(div32(*lhs, *rhs));
                if (e->op == "%") return *rhs == 0 ? nullopt : optional<int32_t>(mod32(*lhs, *rhs));
                if (e->op == "<") return *lhs < *rhs;
                if (e->op == ">") return *lhs > *rhs;
                if (e->op == "<=") return *lhs <= *rhs;
                if (e->op == ">=") return *lhs >= *rhs;
                if (e->op == "==") return *lhs == *rhs;
                if (e->op == "!=") return *lhs != *rhs;
                return nullopt;
            }
        }
        return nullopt;
    }

    static string reverseRelation(const string &op) {
        if (op == "<") return ">";
        if (op == "<=") return ">=";
        if (op == ">") return "<";
        if (op == ">=") return "<=";
        return "";
    }

    static bool extractInductionWithOffset(const Expr *expr, int &key,
                                           string &name, int32_t &offset) {
        if (!expr) return false;
        if (expr->kind == Expr::Kind::Var && !expr->fastGlobal &&
            expr->fastIndex >= 0) {
            key = exprKey(expr);
            name = expr->name;
            offset = 0;
            return true;
        }
        if (expr->kind == Expr::Kind::Unary && expr->op == "+") {
            return extractInductionWithOffset(expr->lhs.get(), key, name,
                                              offset);
        }
        if (expr->kind != Expr::Kind::Binary ||
            (expr->op != "+" && expr->op != "-")) {
            return false;
        }
        int nestedKey = -1;
        string nestedName;
        int32_t nestedOffset = 0;
        if (extractInductionWithOffset(expr->lhs.get(), nestedKey,
                                       nestedName, nestedOffset)) {
            auto constant = foldConstExpr(expr->rhs.get());
            if (!constant) return false;
            key = nestedKey;
            name = std::move(nestedName);
            offset = expr->op == "+"
                ? add32(nestedOffset, *constant)
                : sub32(nestedOffset, *constant);
            return true;
        }
        if (expr->op == "+" &&
            extractInductionWithOffset(expr->rhs.get(), nestedKey,
                                       nestedName, nestedOffset)) {
            auto constant = foldConstExpr(expr->lhs.get());
            if (!constant) return false;
            key = nestedKey;
            name = std::move(nestedName);
            offset = add32(*constant, nestedOffset);
            return true;
        }
        return false;
    }

    bool extractConditionShape(const Expr *condition,
                               CountedLoop &loop) const {
        if (!condition || condition->kind != Expr::Kind::Binary) return false;
        if (condition->op == "&&") {
            if (auto lhs = foldConstExpr(condition->lhs.get());
                lhs && truthy(*lhs)) {
                return extractConditionShape(condition->rhs.get(), loop);
            }
            if (auto rhs = foldConstExpr(condition->rhs.get());
                rhs && truthy(*rhs)) {
                return extractConditionShape(condition->lhs.get(), loop);
            }
            return false;
        }
        if (condition->op == "||") {
            if (auto lhs = foldConstExpr(condition->lhs.get());
                lhs && !truthy(*lhs)) {
                return extractConditionShape(condition->rhs.get(), loop);
            }
            if (auto rhs = foldConstExpr(condition->rhs.get());
                rhs && !truthy(*rhs)) {
                return extractConditionShape(condition->lhs.get(), loop);
            }
            return false;
        }
        static const unordered_set<string> relations = {"<", "<=", ">", ">="};
        if (!relations.count(condition->op)) return false;

        int inductionKey = -1;
        string inductionName;
        int32_t inductionOffset = 0;
        loop.boundExpr = condition->rhs.get();
        loop.relation = condition->op;
        if (!extractInductionWithOffset(condition->lhs.get(), inductionKey,
                                        inductionName, inductionOffset)) {
            loop.boundExpr = condition->lhs.get();
            loop.relation = reverseRelation(condition->op);
            if (!extractInductionWithOffset(condition->rhs.get(), inductionKey,
                                            inductionName,
                                            inductionOffset)) {
                return false;
            }
        }
        if (loop.relation.empty()) return false;
        loop.inductionKey = inductionKey;
        loop.inductionName = std::move(inductionName);
        loop.conditionOffset = inductionOffset;
        return loop.inductionKey >= 0;
    }

    bool extractCondition(const Expr *condition, const ExactEnv &env,
        CountedLoop &loop) const {
        if (!extractConditionShape(condition, loop)) return false;
        optional<int32_t> start;
        if (!isGlobalKey(loop.inductionKey) && loop.inductionKey >= 0 &&
            loop.inductionKey < static_cast<int>(env.size())) {
            start = env[static_cast<size_t>(loop.inductionKey)];
        }
        auto bound = evalExact(loop.boundExpr, env);
        if (!start || !bound) return false;
        loop.start = *start;
        loop.bound = *bound;
        return true;
    }

    static optional<pair<uint64_t, int32_t>> tripCount(const CountedLoop &loop) {
        int64_t start = loop.start;
        int64_t conditionStart = start + loop.conditionOffset;
        if (conditionStart < numeric_limits<int32_t>::min() ||
            conditionStart > numeric_limits<int32_t>::max()) {
            return nullopt;
        }
        int64_t bound = loop.bound;
        int64_t step = loop.step;
        uint64_t trips = 0;
        int64_t finalValue = start;

        if (loop.relation == "<" || loop.relation == "<=") {
            if (step <= 0) return nullopt;
            bool enters = loop.relation == "<"
                ? conditionStart < bound : conditionStart <= bound;
            if (enters) {
                uint64_t distance =
                    static_cast<uint64_t>(bound - conditionStart);
                trips = loop.relation == "<"
                    ? (distance + static_cast<uint64_t>(step) - 1) / static_cast<uint64_t>(step)
                    : distance / static_cast<uint64_t>(step) + 1;
                finalValue = start + static_cast<int64_t>(trips) * step;
            }
        } else if (loop.relation == ">" || loop.relation == ">=") {
            if (step >= 0) return nullopt;
            bool enters = loop.relation == ">"
                ? conditionStart > bound : conditionStart >= bound;
            if (enters) {
                uint64_t magnitude = static_cast<uint64_t>(-step);
                uint64_t distance =
                    static_cast<uint64_t>(conditionStart - bound);
                trips = loop.relation == ">"
                    ? (distance + magnitude - 1) / magnitude
                    : distance / magnitude + 1;
                finalValue = start - static_cast<int64_t>(trips) * static_cast<int64_t>(magnitude);
            }
        } else {
            return nullopt;
        }

        // If the induction update would wrap, the signed relational loop need
        // not terminate at the algebraic trip count.  Keep the original loop.
        if (finalValue < numeric_limits<int32_t>::min() ||
            finalValue > numeric_limits<int32_t>::max()) {
            return nullopt;
        }
        return pair<uint64_t, int32_t>{trips, static_cast<int32_t>(finalValue)};
    }

    static bool exprHasCallLocal(const Expr *e) {
        if (!e) return false;
        if (e->kind == Expr::Kind::Call) return true;
        if (exprHasCallLocal(e->lhs.get()) || exprHasCallLocal(e->rhs.get())) return true;
        for (auto &arg : e->args) {
            if (exprHasCallLocal(arg.get())) return true;
        }
        return false;
    }

    static void collectExprKeys(
        const Expr *e, unordered_set<int> &keys,
        const vector<const Expr *> *params = nullptr) {
        if (!e) return;
        if (e->kind == Expr::Kind::Var) {
            if (params && !e->fastGlobal && e->fastIndex >= 0 &&
                e->fastIndex < static_cast<int>(params->size())) {
                collectExprKeys((*params)[static_cast<size_t>(e->fastIndex)], keys);
                return;
            }
            int key = exprKey(e);
            if (key >= 0) keys.insert(key);
            return;
        }
        collectExprKeys(e->lhs.get(), keys, params);
        collectExprKeys(e->rhs.get(), keys, params);
        for (auto &arg : e->args) collectExprKeys(arg.get(), keys, params);
    }

    static void collectModifiedKeys(const Stmt *s, unordered_set<int> &keys) {
        if (!s) return;
        if (s->kind == Stmt::Kind::Assign) {
            int key = assignKey(s);
            if (key >= 0) keys.insert(key);
        } else if (s->kind == Stmt::Kind::DeclStmt && s->decl && s->decl->fastSlot >= 0) {
            keys.insert(s->decl->fastSlot);
        }
        for (auto &child : s->stmts) collectModifiedKeys(child.get(), keys);
        collectModifiedKeys(s->thenStmt.get(), keys);
        collectModifiedKeys(s->elseStmt.get(), keys);
        collectModifiedKeys(s->body.get(), keys);
    }

    static bool stmtWritesKey(const Stmt *s, int key) {
        if (!s) return false;
        if (s->kind == Stmt::Kind::Assign && assignKey(s) == key) return true;
        if (s->kind == Stmt::Kind::DeclStmt && s->decl && s->decl->fastSlot == key) return true;
        for (auto &child : s->stmts) {
            if (stmtWritesKey(child.get(), key)) return true;
        }
        return stmtWritesKey(s->thenStmt.get(), key) ||
               stmtWritesKey(s->elseStmt.get(), key) ||
               stmtWritesKey(s->body.get(), key);
    }

    bool inductionDelta(const Expr *e, int inductionKey, const ExactEnv &env,
                        int32_t &delta) const {
        if (!e) return false;
        if (e->kind == Expr::Kind::Var && exprKey(e) == inductionKey) {
            delta = 0;
            return true;
        }
        if (e->kind == Expr::Kind::Unary && e->op == "+") {
            return inductionDelta(e->lhs.get(), inductionKey, env, delta);
        }
        if (e->kind != Expr::Kind::Binary) return false;

        int32_t nested = 0;
        if ((e->op == "+" || e->op == "-") &&
            inductionDelta(e->lhs.get(), inductionKey, env, nested)) {
            auto constant = evalExact(e->rhs.get(), env);
            if (!constant) return false;
            delta = e->op == "+" ? add32(nested, *constant)
                                   : sub32(nested, *constant);
            return true;
        }
        if (e->op == "+" && inductionDelta(e->rhs.get(), inductionKey, env, nested)) {
            auto constant = evalExact(e->lhs.get(), env);
            if (!constant) return false;
            delta = add32(*constant, nested);
            return true;
        }
        return false;
    }

    bool findUnconditionalStep(const Stmt *s, int inductionKey, const ExactEnv &env,
                               optional<int32_t> &step) const {
        if (!s) return true;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    if (!findUnconditionalStep(child.get(), inductionKey, env, step)) return false;
                }
                return true;
            case Stmt::Kind::Empty:
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::DeclStmt:
                return true;
            case Stmt::Kind::Assign:
                if (assignKey(s) != inductionKey) return true;
                if (step) return false;
                int32_t delta;
                if (!inductionDelta(s->expr.get(), inductionKey, env, delta) || delta == 0) {
                    return false;
                }
                step = delta;
                return true;
            case Stmt::Kind::If:
                // A path-dependent induction update changes the trip count.
                return !stmtWritesKey(s, inductionKey);
            case Stmt::Kind::While:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
            case Stmt::Kind::Return:
                return false;
        }
        return false;
    }

    bool tryDropDeadLoop(unique_ptr<Stmt> &stmt, const ExactEnv &env,
                         CountedLoop &counted, unordered_set<int> &modifiedLocals) {
        if (!stmt || stmt->kind != Stmt::Kind::While || !stmt->fastLoopValuesDead ||
            !extractConditionShape(stmt->expr.get(), counted)) {
            return false;
        }

        unordered_set<int> modified;
        collectModifiedKeys(stmt->body.get(), modified);
        unordered_set<int> boundKeys;
        collectExprKeys(counted.boundExpr, boundKeys);
        for (int key : modified) {
            if (boundKeys.count(key)) return false;
        }

        optional<int32_t> step;
        if (!findUnconditionalStep(stmt->body.get(), counted.inductionKey, env, step) || !step) {
            return false;
        }
        counted.step = *step;

        const Expr *induction = stmt->expr->lhs.get();
        if (exprKey(induction) != counted.inductionKey) {
            induction = stmt->expr->rhs.get();
        }
        auto start = evalExact(induction, env);
        auto bound = evalExact(counted.boundExpr, env);
        if (start && bound) {
            counted.start = *start;
            counted.bound = *bound;
            auto trip = tripCount(counted);
            if (!trip) return false;
            counted.trips = trip->first;
            counted.finalValue = trip->second;
        } else {
            // ToyC inputs are guaranteed to contain no undefined signed
            // overflow and to terminate.  Under those language preconditions,
            // a positive induction step is monotone for < / <= and a negative
            // step is monotone for > / >=, even when the endpoints are only
            // available at runtime.
            bool increasing =
                (counted.relation == "<" || counted.relation == "<=") &&
                counted.step > 0;
            bool decreasing =
                (counted.relation == ">" || counted.relation == ">=") &&
                counted.step < 0;
            if (!increasing && !decreasing) return false;
        }

        for (int key : modified) {
            if (!isGlobalKey(key)) modifiedLocals.insert(key);
        }
        auto empty = make_unique<Stmt>();
        empty->kind = Stmt::Kind::Empty;
        stmt = std::move(empty);
        changed = true;
        return true;
    }

    static void addModelKey(Model &model, int key, const string &name) {
        if (key < 0 || model.index.count(key)) return;
        model.index[key] = static_cast<int>(model.keys.size());
        model.keys.push_back(key);
        model.names[key] = name;
    }

    const Function *branchHelperCall(const Expr *e) const {
        if (!e || e->kind != Expr::Kind::Call) return nullptr;
        auto found = branchHelpers.find(e->name);
        if (found == branchHelpers.end() ||
            e->args.size() != found->second->params.size()) {
            return nullptr;
        }
        for (auto &arg : e->args) {
            if (exprHasCallLocal(arg.get())) return nullptr;
        }
        return found->second;
    }

    const Function *exactLoopHelperCallShape(const Expr *e) const {
        if (!e || e->kind != Expr::Kind::Call) return nullptr;
        auto found = exactLoopHelpers.find(e->name);
        if (found == exactLoopHelpers.end() ||
            e->args.size() != found->second->params.size()) {
            return nullptr;
        }
        for (auto &arg : e->args) {
            if (exprHasCallLocal(arg.get())) return nullptr;
        }
        return found->second;
    }

    bool collectExprVars(const Expr *e, Model &model) const {
        if (!e) return true;
        if (e->kind == Expr::Kind::Call) {
            if (!branchHelperCall(e) && !exactLoopHelperCallShape(e)) return false;
            for (auto &arg : e->args) {
                if (!collectExprVars(arg.get(), model)) return false;
            }
            return true;
        }
        if (e->kind == Expr::Kind::Var) {
            int key = exprKey(e);
            if (key < 0) return false;
            addModelKey(model, key, e->name);
            return true;
        }
        if (!collectExprVars(e->lhs.get(), model) || !collectExprVars(e->rhs.get(), model)) {
            return false;
        }
        for (auto &arg : e->args) {
            if (!collectExprVars(arg.get(), model)) return false;
        }
        return true;
    }

    bool collectBody(const Stmt *s, Model &model) const {
        if (!s || s->fastDeadStore) return true;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    if (!collectBody(child.get(), model)) return false;
                }
                return true;
            case Stmt::Kind::Empty:
                return true;
            case Stmt::Kind::Assign: {
                int key = assignKey(s);
                if (key < 0 || !collectExprVars(s->expr.get(), model)) return false;
                addModelKey(model, key, s->name);
                model.modified.insert(key);
                return true;
            }
            case Stmt::Kind::DeclStmt: {
                if (!s->decl || s->decl->fastSlot < 0 ||
                    !collectExprVars(s->decl->init.get(), model)) {
                    return false;
                }
                int key = s->decl->fastSlot;
                addModelKey(model, key, s->decl->name);
                model.modified.insert(key);
                model.transient.insert(key);
                return true;
            }
            case Stmt::Kind::ExprStmt:
                return collectExprVars(s->expr.get(), model);
            case Stmt::Kind::If:
                return collectExprVars(s->expr.get(), model) &&
                       collectBody(s->thenStmt.get(), model) &&
                       collectBody(s->elseStmt.get(), model);
            case Stmt::Kind::While:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
            case Stmt::Kind::Return:
                return false;
        }
        return false;
    }

    HelperFlow exactHelperStmt(
        const Stmt *s, const vector<const Expr *> &args,
        const Model &model, const Matrix &current,
        const vector<optional<int32_t>> *base, optional<int32_t> &result) const {
        if (!s) return HelperFlow::Fallthrough;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    HelperFlow flow = exactHelperStmt(child.get(), args, model,
                                                      current, base, result);
                    if (flow != HelperFlow::Fallthrough) return flow;
                }
                return HelperFlow::Fallthrough;
            case Stmt::Kind::Empty:
                return HelperFlow::Fallthrough;
            case Stmt::Kind::If: {
                auto condition = exactTransformExpr(s->expr.get(), model, current,
                                                    base, &args);
                if (!condition) return HelperFlow::Fail;
                const Stmt *branch = truthy(*condition) ? s->thenStmt.get()
                                                        : s->elseStmt.get();
                return branch ? exactHelperStmt(branch, args, model, current,
                                                 base, result)
                              : HelperFlow::Fallthrough;
            }
            case Stmt::Kind::Return:
                result = exactTransformExpr(s->expr.get(), model, current, base, &args);
                return result ? HelperFlow::Returned : HelperFlow::Fail;
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Assign:
            case Stmt::Kind::DeclStmt:
            case Stmt::Kind::While:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return HelperFlow::Fail;
        }
        return HelperFlow::Fail;
    }

    bool applyClosedToExactEnv(const Model &model, const Matrix &closed,
                               ExactEnv &env) const {
        ExactEnv next = env;
        for (int key : model.modified) {
            if (model.transient.count(key)) continue;
            if (isGlobalKey(key) || key < 0 || key >= static_cast<int>(env.size())) {
                return false;
            }
            const Row &row = closed[static_cast<size_t>(model.index.at(key))];
            uint32_t value = row.back();
            for (size_t i = 0; i < model.keys.size(); ++i) {
                uint32_t coefficient = row[i];
                if (coefficient == 0) continue;
                int sourceKey = model.keys[i];
                if (model.transient.count(sourceKey) || isGlobalKey(sourceKey) ||
                    sourceKey < 0 || sourceKey >= static_cast<int>(env.size()) ||
                    !env[static_cast<size_t>(sourceKey)]) {
                    return false;
                }
                value += static_cast<uint32_t>(
                    static_cast<uint64_t>(coefficient) *
                    static_cast<uint32_t>(*env[static_cast<size_t>(sourceKey)]));
            }
            next[static_cast<size_t>(key)] = static_cast<int32_t>(value);
        }
        env = std::move(next);
        return true;
    }

    bool summarizeExactHelperLoop(const Stmt *loop, ExactEnv &env) const {
        CountedLoop counted;
        if (!loop || loop->kind != Stmt::Kind::While || !loop->expr ||
            !extractCondition(loop->expr.get(), env, counted)) {
            return false;
        }

        Model model;
        addModelKey(model, counted.inductionKey, counted.inductionName);
        if (!collectBody(loop->body.get(), model) || model.keys.size() > 32 ||
            !model.modified.count(counted.inductionKey)) {
            return false;
        }

        unordered_set<int> boundKeys;
        Model boundModel;
        if (!collectExprVars(counted.boundExpr, boundModel)) return false;
        for (int key : boundModel.keys) boundKeys.insert(key);
        for (int key : model.modified) {
            if (isGlobalKey(key) || boundKeys.count(key)) return false;
        }

        const int states = static_cast<int>(model.keys.size());
        Matrix transform = identityMatrix(states + 1);
        Matrix closed;
        if (applyBodyTransform(loop->body.get(), model, transform)) {
            int inductionIndex = model.index.at(counted.inductionKey);
            const Row &inductionRow = transform[static_cast<size_t>(inductionIndex)];
            for (int i = 0; i < states; ++i) {
                uint32_t expected = i == inductionIndex ? 1u : 0u;
                if (inductionRow[static_cast<size_t>(i)] != expected) return false;
            }
            counted.step = static_cast<int32_t>(inductionRow.back());
            auto trip = tripCount(counted);
            if (!trip) return false;
            counted.trips = trip->first;
            counted.finalValue = trip->second;
            closed = power(std::move(transform), counted.trips);
        } else if (!buildPeriodicClosed(loop, env, model, counted, closed, false)) {
            return false;
        }
        return applyClosedToExactEnv(model, closed, env);
    }

    HelperFlow summarizeExactHelperStmt(const Stmt *s, ExactEnv &env,
                                        optional<int32_t> &result) const {
        if (!s) return HelperFlow::Fallthrough;
        if (s->fastDeadStore && (s->kind == Stmt::Kind::ExprStmt ||
                                 s->kind == Stmt::Kind::Assign ||
                                 s->kind == Stmt::Kind::DeclStmt)) {
            return HelperFlow::Fallthrough;
        }
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    HelperFlow flow = summarizeExactHelperStmt(child.get(), env, result);
                    if (flow != HelperFlow::Fallthrough) return flow;
                }
                return HelperFlow::Fallthrough;
            case Stmt::Kind::Empty:
            case Stmt::Kind::ExprStmt:
                return HelperFlow::Fallthrough;
            case Stmt::Kind::DeclStmt: {
                if (!s->decl || s->decl->fastSlot < 0 ||
                    s->decl->fastSlot >= static_cast<int>(env.size())) {
                    return HelperFlow::Fail;
                }
                auto value = evalExact(s->decl->init.get(), env);
                if (!value) return HelperFlow::Fail;
                env[static_cast<size_t>(s->decl->fastSlot)] = *value;
                return HelperFlow::Fallthrough;
            }
            case Stmt::Kind::Assign: {
                if (s->fastAssignGlobal || s->fastAssignIndex < 0 ||
                    s->fastAssignIndex >= static_cast<int>(env.size())) {
                    return HelperFlow::Fail;
                }
                auto value = evalExact(s->expr.get(), env);
                if (!value) return HelperFlow::Fail;
                env[static_cast<size_t>(s->fastAssignIndex)] = *value;
                return HelperFlow::Fallthrough;
            }
            case Stmt::Kind::If: {
                auto condition = evalExact(s->expr.get(), env);
                if (!condition) return HelperFlow::Fail;
                const Stmt *branch = truthy(*condition) ? s->thenStmt.get()
                                                        : s->elseStmt.get();
                return branch ? summarizeExactHelperStmt(branch, env, result)
                              : HelperFlow::Fallthrough;
            }
            case Stmt::Kind::While:
                return summarizeExactHelperLoop(s, env)
                    ? HelperFlow::Fallthrough : HelperFlow::Fail;
            case Stmt::Kind::Return:
                result = evalExact(s->expr.get(), env);
                return result ? HelperFlow::Returned : HelperFlow::Fail;
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return HelperFlow::Fail;
        }
        return HelperFlow::Fail;
    }

    optional<int32_t> exactLoopHelperValue(
        const Expr *call, const Model &model, const Matrix &current,
        const vector<optional<int32_t>> *base) const {
        const Function *function = exactLoopHelperCallShape(call);
        if (!function) return nullopt;
        ExactEnv env(static_cast<size_t>(function->fastLocalCount));
        for (size_t i = 0; i < call->args.size(); ++i) {
            auto value = exactTransformExpr(call->args[i].get(), model, current, base);
            if (!value) return nullopt;
            env[i] = *value;
        }
        optional<int32_t> result;
        return summarizeExactHelperStmt(function->body.get(), env, result) ==
                       HelperFlow::Returned
            ? result : nullopt;
    }

    optional<int32_t> exactHelperCall(
        const Expr *call, const Model &model, const Matrix &current,
        const vector<optional<int32_t>> *base) const {
        if (const Function *function = branchHelperCall(call)) {
            vector<const Expr *> args;
            args.reserve(call->args.size());
            for (auto &arg : call->args) args.push_back(arg.get());
            optional<int32_t> result;
            if (exactHelperStmt(function->body.get(), args, model, current,
                                base, result) == HelperFlow::Returned) {
                return result;
            }
        }
        return exactLoopHelperValue(call, model, current, base);
    }

    optional<int32_t> exactTransformExpr(
        const Expr *e, const Model &model, const Matrix &current,
        const vector<optional<int32_t>> *base = nullptr,
        const vector<const Expr *> *params = nullptr) const {
        if (!e) return nullopt;
        switch (e->kind) {
            case Expr::Kind::Number:
                return wrap32(e->value);
            case Expr::Kind::Var: {
                if (params && !e->fastGlobal && e->fastIndex >= 0 &&
                    e->fastIndex < static_cast<int>(params->size())) {
                    return exactTransformExpr(
                        (*params)[static_cast<size_t>(e->fastIndex)],
                        model, current, base);
                }
                auto found = model.index.find(exprKey(e));
                if (found == model.index.end()) return nullopt;
                const Row &row = current[static_cast<size_t>(found->second)];
                if (constRow(row)) return static_cast<int32_t>(row.back());
                if (!base || base->size() != row.size()) return nullopt;
                uint32_t value = 0;
                for (size_t i = 0; i < row.size(); ++i) {
                    if (row[i] == 0) continue;
                    if (!(*base)[i]) return nullopt;
                    value += static_cast<uint32_t>(
                        static_cast<uint64_t>(row[i]) *
                        static_cast<uint32_t>(*(*base)[i]));
                }
                return static_cast<int32_t>(value);
            }
            case Expr::Kind::Call:
                return params ? nullopt : exactHelperCall(e, model, current, base);
            case Expr::Kind::Unary: {
                auto value = exactTransformExpr(e->lhs.get(), model, current, base, params);
                if (!value) return nullopt;
                if (e->op == "+") return *value;
                if (e->op == "-") return sub32(0, *value);
                if (e->op == "!") return !truthy(*value);
                return nullopt;
            }
            case Expr::Kind::Binary: {
                if (e->op == "&&") {
                    auto lhs = exactTransformExpr(e->lhs.get(), model, current, base, params);
                    if (!lhs) return nullopt;
                    if (!truthy(*lhs)) return 0;
                    auto rhs = exactTransformExpr(e->rhs.get(), model, current, base, params);
                    return rhs ? optional<int32_t>(truthy(*rhs)) : nullopt;
                }
                if (e->op == "||") {
                    auto lhs = exactTransformExpr(e->lhs.get(), model, current, base, params);
                    if (!lhs) return nullopt;
                    if (truthy(*lhs)) return 1;
                    auto rhs = exactTransformExpr(e->rhs.get(), model, current, base, params);
                    return rhs ? optional<int32_t>(truthy(*rhs)) : nullopt;
                }
                auto lhs = exactTransformExpr(e->lhs.get(), model, current, base, params);
                auto rhs = exactTransformExpr(e->rhs.get(), model, current, base, params);
                if (!lhs || !rhs) return nullopt;
                if (e->op == "+") return add32(*lhs, *rhs);
                if (e->op == "-") return sub32(*lhs, *rhs);
                if (e->op == "*") return mul32(*lhs, *rhs);
                if (e->op == "/") return *rhs == 0 ? nullopt : optional<int32_t>(div32(*lhs, *rhs));
                if (e->op == "%") return *rhs == 0 ? nullopt : optional<int32_t>(mod32(*lhs, *rhs));
                if (e->op == "<") return *lhs < *rhs;
                if (e->op == ">") return *lhs > *rhs;
                if (e->op == "<=") return *lhs <= *rhs;
                if (e->op == ">=") return *lhs >= *rhs;
                if (e->op == "==") return *lhs == *rhs;
                if (e->op == "!=") return *lhs != *rhs;
                return nullopt;
            }
        }
        return nullopt;
    }

    bool applyBodyTransform(
        const Stmt *s, const Model &model, Matrix &transform,
        const vector<optional<int32_t>> *base = nullptr) const {
        if (!s || s->fastDeadStore) return true;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    if (!applyBodyTransform(child.get(), model, transform, base)) return false;
                }
                return true;
            case Stmt::Kind::Empty:
            case Stmt::Kind::ExprStmt:
                return true;
            case Stmt::Kind::Assign: {
                int key = assignKey(s);
                Row row;
                if (key < 0 ||
                    !affineExpr(s->expr.get(), model, transform, row, base)) {
                    return false;
                }
                transform[static_cast<size_t>(model.index.at(key))] = std::move(row);
                return true;
            }
            case Stmt::Kind::DeclStmt: {
                if (!s->decl || s->decl->fastSlot < 0) return false;
                Row row;
                if (!affineExpr(s->decl->init.get(), model, transform, row, base)) {
                    return false;
                }
                transform[static_cast<size_t>(model.index.at(s->decl->fastSlot))] = std::move(row);
                return true;
            }
            case Stmt::Kind::If: {
                auto condition = exactTransformExpr(s->expr.get(), model, transform, base);
                if (!condition) return false;
                return applyBodyTransform(truthy(*condition) ? s->thenStmt.get()
                                                            : s->elseStmt.get(),
                                          model, transform, base);
            }
            case Stmt::Kind::While:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
            case Stmt::Kind::Return:
                return false;
        }
        return false;
    }

    static bool constRow(const Row &row) {
        for (size_t i = 0; i + 1 < row.size(); ++i) {
            if (row[i] != 0) return false;
        }
        return true;
    }

    HelperFlow affineHelperStmt(
        const Stmt *s, const vector<const Expr *> &args,
        const Model &model, const Matrix &current,
        const vector<optional<int32_t>> *base, Row &result) const {
        if (!s) return HelperFlow::Fallthrough;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    HelperFlow flow = affineHelperStmt(child.get(), args, model,
                                                       current, base, result);
                    if (flow != HelperFlow::Fallthrough) return flow;
                }
                return HelperFlow::Fallthrough;
            case Stmt::Kind::Empty:
                return HelperFlow::Fallthrough;
            case Stmt::Kind::If: {
                auto condition = exactTransformExpr(s->expr.get(), model, current,
                                                    base, &args);
                if (!condition) return HelperFlow::Fail;
                const Stmt *branch = truthy(*condition) ? s->thenStmt.get()
                                                        : s->elseStmt.get();
                return branch ? affineHelperStmt(branch, args, model, current,
                                                  base, result)
                              : HelperFlow::Fallthrough;
            }
            case Stmt::Kind::Return:
                return affineExpr(s->expr.get(), model, current, result, base, &args)
                    ? HelperFlow::Returned : HelperFlow::Fail;
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Assign:
            case Stmt::Kind::DeclStmt:
            case Stmt::Kind::While:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return HelperFlow::Fail;
        }
        return HelperFlow::Fail;
    }

    bool affineHelperCall(const Expr *call, const Model &model,
                          const Matrix &current, Row &out,
                          const vector<optional<int32_t>> *base) const {
        if (const Function *function = branchHelperCall(call)) {
            vector<const Expr *> args;
            args.reserve(call->args.size());
            for (auto &arg : call->args) args.push_back(arg.get());
            if (affineHelperStmt(function->body.get(), args, model, current,
                                 base, out) == HelperFlow::Returned) {
                return true;
            }
        }
        auto exact = exactLoopHelperValue(call, model, current, base);
        if (!exact) return false;
        out.assign(current.size(), 0);
        out.back() = static_cast<uint32_t>(*exact);
        return true;
    }

    bool affineExpr(const Expr *e, const Model &model,
                    const Matrix &current, Row &out,
                    const vector<optional<int32_t>> *base = nullptr,
                    const vector<const Expr *> *params = nullptr) const {
        const int dimension = static_cast<int>(current.size());
        out.assign(static_cast<size_t>(dimension), 0);
        if (!e) return false;
        switch (e->kind) {
            case Expr::Kind::Number:
                out.back() = static_cast<uint32_t>(wrap32(e->value));
                return true;
            case Expr::Kind::Var: {
                if (params && !e->fastGlobal && e->fastIndex >= 0 &&
                    e->fastIndex < static_cast<int>(params->size())) {
                    return affineExpr((*params)[static_cast<size_t>(e->fastIndex)],
                                      model, current, out, base);
                }
                auto found = model.index.find(exprKey(e));
                if (found == model.index.end()) return false;
                out = current[static_cast<size_t>(found->second)];
                return true;
            }
            case Expr::Kind::Call:
                return !params && affineHelperCall(e, model, current, out, base);
            case Expr::Kind::Unary: {
                Row value;
                if (!affineExpr(e->lhs.get(), model, current, value, base, params)) return false;
                if (e->op == "+") {
                    out = std::move(value);
                    return true;
                }
                if (e->op == "-") {
                    for (int i = 0; i < dimension; ++i) out[static_cast<size_t>(i)] = 0u - value[static_cast<size_t>(i)];
                    return true;
                }
                if (base) {
                    if (auto exact = exactTransformExpr(e, model, current, base, params)) {
                        out.assign(static_cast<size_t>(dimension), 0);
                        out.back() = static_cast<uint32_t>(*exact);
                        return true;
                    }
                }
                return false;
            }
            case Expr::Kind::Binary: {
                Row lhs, rhs;
                bool lhsAffine = affineExpr(e->lhs.get(), model, current, lhs, base, params);
                bool rhsAffine = affineExpr(e->rhs.get(), model, current, rhs, base, params);
                if (e->op == "+" || e->op == "-") {
                    if (!lhsAffine || !rhsAffine) return false;
                    for (int i = 0; i < dimension; ++i) {
                        out[static_cast<size_t>(i)] = e->op == "+"
                            ? lhs[static_cast<size_t>(i)] + rhs[static_cast<size_t>(i)]
                            : lhs[static_cast<size_t>(i)] - rhs[static_cast<size_t>(i)];
                    }
                    return true;
                }
                if (e->op == "*") {
                    if (!lhsAffine || !rhsAffine) return false;
                    if (constRow(lhs)) {
                        uint32_t factor = lhs.back();
                        for (int i = 0; i < dimension; ++i) {
                            out[static_cast<size_t>(i)] = static_cast<uint32_t>(
                                static_cast<uint64_t>(rhs[static_cast<size_t>(i)]) * factor);
                        }
                        return true;
                    }
                    if (constRow(rhs)) {
                        uint32_t factor = rhs.back();
                        for (int i = 0; i < dimension; ++i) {
                            out[static_cast<size_t>(i)] = static_cast<uint32_t>(
                                static_cast<uint64_t>(lhs[static_cast<size_t>(i)]) * factor);
                        }
                        return true;
                    }
                }
                if (base) {
                    if (auto exact = exactTransformExpr(e, model, current, base, params)) {
                        out.assign(static_cast<size_t>(dimension), 0);
                        out.back() = static_cast<uint32_t>(*exact);
                        return true;
                    }
                }
                return false;
            }
        }
        return false;
    }

    static Matrix multiply(const Matrix &lhs, const Matrix &rhs) {
        const int n = static_cast<int>(lhs.size());
        Matrix out(static_cast<size_t>(n), Row(static_cast<size_t>(n), 0));
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < n; ++k) {
                uint32_t factor = lhs[static_cast<size_t>(i)][static_cast<size_t>(k)];
                if (factor == 0) continue;
                for (int j = 0; j < n; ++j) {
                    out[static_cast<size_t>(i)][static_cast<size_t>(j)] +=
                        static_cast<uint32_t>(static_cast<uint64_t>(factor) *
                                              rhs[static_cast<size_t>(k)][static_cast<size_t>(j)]);
                }
            }
        }
        return out;
    }

    static Matrix power(Matrix base, uint64_t exponent) {
        const int n = static_cast<int>(base.size());
        Matrix result(static_cast<size_t>(n), Row(static_cast<size_t>(n), 0));
        for (int i = 0; i < n; ++i) result[static_cast<size_t>(i)][static_cast<size_t>(i)] = 1;
        while (exponent != 0) {
            if (exponent & 1u) result = multiply(result, base);
            exponent >>= 1u;
            if (exponent != 0) base = multiply(base, base);
        }
        return result;
    }

    static Matrix identityMatrix(int dimension) {
        Matrix result(static_cast<size_t>(dimension),
                      Row(static_cast<size_t>(dimension), 0));
        for (int i = 0; i < dimension; ++i) {
            result[static_cast<size_t>(i)][static_cast<size_t>(i)] = 1;
        }
        return result;
    }

    bool hasExactInvariantValue(int key, const ExactEnv &env,
                                const Model &model) const {
        if (key < 0 || model.modified.count(key)) return false;
        if (isGlobalKey(key)) return exactGlobals.count(globalIndex(key)) != 0;
        return key < static_cast<int>(env.size()) && env[static_cast<size_t>(key)].has_value();
    }

    HelperFlow periodicHelperStmt(
        const Stmt *s, const vector<const Expr *> &args,
        int inductionKey, const ExactEnv &env, const Model &model,
        const unordered_set<int> &phaseExactKeys,
        vector<int32_t> &moduli, bool &sawPeriodic,
        bool requireExactReturn) const {
        if (!s) return HelperFlow::Fallthrough;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    HelperFlow flow = periodicHelperStmt(
                        child.get(), args, inductionKey, env, model,
                        phaseExactKeys, moduli, sawPeriodic, requireExactReturn);
                    if (flow != HelperFlow::Fallthrough) return flow;
                }
                return HelperFlow::Fallthrough;
            case Stmt::Kind::Empty:
                return HelperFlow::Fallthrough;
            case Stmt::Kind::If: {
                if (!phaseExactExpr(s->expr.get(), inductionKey, env, model,
                                    phaseExactKeys, moduli, sawPeriodic, &args)) {
                    return HelperFlow::Fail;
                }
                HelperFlow thenFlow = periodicHelperStmt(
                    s->thenStmt.get(), args, inductionKey, env, model,
                    phaseExactKeys, moduli, sawPeriodic, requireExactReturn);
                if (thenFlow == HelperFlow::Fail) return HelperFlow::Fail;
                HelperFlow elseFlow = s->elseStmt
                    ? periodicHelperStmt(s->elseStmt.get(), args, inductionKey,
                                         env, model, phaseExactKeys, moduli,
                                         sawPeriodic, requireExactReturn)
                    : HelperFlow::Fallthrough;
                if (elseFlow == HelperFlow::Fail) return HelperFlow::Fail;
                return thenFlow == HelperFlow::Returned &&
                               elseFlow == HelperFlow::Returned
                    ? HelperFlow::Returned : HelperFlow::Fallthrough;
            }
            case Stmt::Kind::Return: {
                bool valid = requireExactReturn
                    ? phaseExactExpr(s->expr.get(), inductionKey, env, model,
                                     phaseExactKeys, moduli, sawPeriodic, &args)
                    : periodicAffineExpr(s->expr.get(), inductionKey, env, model,
                                         phaseExactKeys, moduli, sawPeriodic, &args);
                return valid ? HelperFlow::Returned : HelperFlow::Fail;
            }
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Assign:
            case Stmt::Kind::DeclStmt:
            case Stmt::Kind::While:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return HelperFlow::Fail;
        }
        return HelperFlow::Fail;
    }

    bool periodicHelperCall(
        const Expr *call, int inductionKey, const ExactEnv &env,
        const Model &model, const unordered_set<int> &phaseExactKeys,
        vector<int32_t> &moduli, bool &sawPeriodic,
        bool requireExactReturn) const {
        const Function *function = branchHelperCall(call);
        if (!function) return false;
        vector<const Expr *> args;
        args.reserve(call->args.size());
        for (auto &arg : call->args) args.push_back(arg.get());
        return periodicHelperStmt(function->body.get(), args, inductionKey,
                                  env, model, phaseExactKeys, moduli,
                                  sawPeriodic, requireExactReturn) ==
               HelperFlow::Returned;
    }

    bool phaseExactExpr(const Expr *e, int inductionKey, const ExactEnv &env,
                        const Model &model, const unordered_set<int> &phaseExactKeys,
                        vector<int32_t> &moduli, bool &sawPeriodic,
                        const vector<const Expr *> *params = nullptr) const {
        if (!e) return false;
        switch (e->kind) {
            case Expr::Kind::Number:
                return true;
            case Expr::Kind::Var: {
                if (params && !e->fastGlobal && e->fastIndex >= 0 &&
                    e->fastIndex < static_cast<int>(params->size())) {
                    return phaseExactExpr(
                        (*params)[static_cast<size_t>(e->fastIndex)],
                        inductionKey, env, model, phaseExactKeys,
                        moduli, sawPeriodic);
                }
                int key = exprKey(e);
                return phaseExactKeys.count(key) ||
                       (key != inductionKey && hasExactInvariantValue(key, env, model));
            }
            case Expr::Kind::Call:
                if (params) return false;
                if (periodicHelperCall(e, inductionKey, env, model,
                                       phaseExactKeys, moduli,
                                       sawPeriodic, true)) {
                    return true;
                }
                if (!exactLoopHelperCallShape(e)) return false;
                for (auto &arg : e->args) {
                    if (!phaseExactExpr(arg.get(), inductionKey, env, model,
                                        phaseExactKeys, moduli, sawPeriodic)) {
                        return false;
                    }
                }
                return true;
            case Expr::Kind::Unary:
                return phaseExactExpr(e->lhs.get(), inductionKey, env, model,
                                      phaseExactKeys, moduli, sawPeriodic, params);
            case Expr::Kind::Binary:
                if (e->op == "%") {
                    const Expr *dividend = e->lhs.get();
                    if (params && dividend && dividend->kind == Expr::Kind::Var &&
                        !dividend->fastGlobal && dividend->fastIndex >= 0 &&
                        dividend->fastIndex < static_cast<int>(params->size())) {
                        dividend = (*params)[static_cast<size_t>(dividend->fastIndex)];
                    }
                    if (exprKey(dividend) != inductionKey) {
                        return phaseExactExpr(e->lhs.get(), inductionKey, env, model,
                                              phaseExactKeys, moduli, sawPeriodic, params) &&
                               phaseExactExpr(e->rhs.get(), inductionKey, env, model,
                                              phaseExactKeys, moduli, sawPeriodic, params);
                    }
                    unordered_set<int> divisorKeys;
                    collectExprKeys(e->rhs.get(), divisorKeys, params);
                    auto divisor = evalExact(e->rhs.get(), env, params);
                    if (!divisor || *divisor <= 0 || divisorKeys.count(inductionKey)) {
                        return false;
                    }
                    moduli.push_back(*divisor);
                    sawPeriodic = true;
                    return true;
                }
                return phaseExactExpr(e->lhs.get(), inductionKey, env, model,
                                      phaseExactKeys, moduli, sawPeriodic, params) &&
                       phaseExactExpr(e->rhs.get(), inductionKey, env, model,
                                      phaseExactKeys, moduli, sawPeriodic, params);
        }
        return false;
    }

    bool periodicAffineExpr(const Expr *e, int inductionKey, const ExactEnv &env,
                            const Model &model,
                            const unordered_set<int> &phaseExactKeys,
                            vector<int32_t> &moduli, bool &sawPeriodic,
                            const vector<const Expr *> *params = nullptr) const {
        if (!e) return false;
        if (params && e->kind == Expr::Kind::Var && !e->fastGlobal &&
            e->fastIndex >= 0 && e->fastIndex < static_cast<int>(params->size())) {
            return periodicAffineExpr(
                (*params)[static_cast<size_t>(e->fastIndex)], inductionKey,
                env, model, phaseExactKeys, moduli, sawPeriodic);
        }
        if (phaseExactExpr(e, inductionKey, env, model, phaseExactKeys,
                           moduli, sawPeriodic, params)) {
            return true;
        }
        if (e->kind == Expr::Kind::Number || e->kind == Expr::Kind::Var) return true;
        if (e->kind == Expr::Kind::Call) {
            return !params && periodicHelperCall(
                e, inductionKey, env, model, phaseExactKeys,
                moduli, sawPeriodic, false);
        }
        if (e->kind == Expr::Kind::Unary) {
            return (e->op == "+" || e->op == "-") &&
                   periodicAffineExpr(e->lhs.get(), inductionKey, env, model,
                                      phaseExactKeys, moduli, sawPeriodic, params);
        }
        if (e->op == "+" || e->op == "-") {
            return periodicAffineExpr(e->lhs.get(), inductionKey, env, model,
                                      phaseExactKeys, moduli, sawPeriodic, params) &&
                   periodicAffineExpr(e->rhs.get(), inductionKey, env, model,
                                      phaseExactKeys, moduli, sawPeriodic, params);
        }
        if (e->op == "*") {
            bool lhsExact = phaseExactExpr(e->lhs.get(), inductionKey, env, model,
                                           phaseExactKeys, moduli, sawPeriodic, params);
            if (lhsExact && periodicAffineExpr(e->rhs.get(), inductionKey, env, model,
                                               phaseExactKeys, moduli, sawPeriodic, params)) {
                return true;
            }
            bool rhsExact = phaseExactExpr(e->rhs.get(), inductionKey, env, model,
                                           phaseExactKeys, moduli, sawPeriodic, params);
            return rhsExact &&
                   periodicAffineExpr(e->lhs.get(), inductionKey, env, model,
                                      phaseExactKeys, moduli, sawPeriodic, params);
        }
        return false;
    }

    bool collectPeriodicStructure(const Stmt *s, int inductionKey,
                                  const ExactEnv &env, const Model &model,
                                  unordered_set<int> &phaseExactKeys,
                                  vector<int32_t> &moduli,
                                  bool &sawPeriodic) const {
        if (!s || s->fastDeadStore) return true;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    if (!collectPeriodicStructure(child.get(), inductionKey, env, model,
                                                  phaseExactKeys, moduli, sawPeriodic)) {
                        return false;
                    }
                }
                return true;
            case Stmt::Kind::Empty:
            case Stmt::Kind::ExprStmt:
                return true;
            case Stmt::Kind::Assign: {
                int key = assignKey(s);
                if (key < 0 ||
                    !periodicAffineExpr(s->expr.get(), inductionKey, env, model,
                                        phaseExactKeys, moduli, sawPeriodic)) {
                    return false;
                }
                if (phaseExactExpr(s->expr.get(), inductionKey, env, model,
                                   phaseExactKeys, moduli, sawPeriodic)) {
                    phaseExactKeys.insert(key);
                } else {
                    phaseExactKeys.erase(key);
                }
                return true;
            }
            case Stmt::Kind::DeclStmt: {
                if (!s->decl || s->decl->fastSlot < 0 ||
                    !periodicAffineExpr(s->decl->init.get(), inductionKey, env, model,
                                        phaseExactKeys, moduli, sawPeriodic)) {
                    return false;
                }
                int key = s->decl->fastSlot;
                if (phaseExactExpr(s->decl->init.get(), inductionKey, env, model,
                                   phaseExactKeys, moduli, sawPeriodic)) {
                    phaseExactKeys.insert(key);
                } else {
                    phaseExactKeys.erase(key);
                }
                return true;
            }
            case Stmt::Kind::If: {
                if (!phaseExactExpr(s->expr.get(), inductionKey, env, model,
                                    phaseExactKeys, moduli, sawPeriodic)) {
                    return false;
                }
                auto thenExact = phaseExactKeys;
                auto elseExact = phaseExactKeys;
                if (!collectPeriodicStructure(s->thenStmt.get(), inductionKey, env, model,
                                              thenExact, moduli, sawPeriodic) ||
                    !collectPeriodicStructure(s->elseStmt.get(), inductionKey, env, model,
                                              elseExact, moduli, sawPeriodic)) {
                    return false;
                }
                for (auto it = thenExact.begin(); it != thenExact.end();) {
                    if (!elseExact.count(*it)) it = thenExact.erase(it);
                    else ++it;
                }
                phaseExactKeys = std::move(thenExact);
                return true;
            }
            case Stmt::Kind::While:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
            case Stmt::Kind::Return:
                return false;
        }
        return false;
    }

    vector<optional<int32_t>> periodicBase(const Model &model,
                                           const ExactEnv &env,
                                           int inductionKey,
                                           int32_t inductionValue) const {
        vector<optional<int32_t>> base(model.keys.size() + 1);
        for (size_t i = 0; i < model.keys.size(); ++i) {
            int key = model.keys[i];
            if (key == inductionKey) {
                base[i] = inductionValue;
            } else if (model.modified.count(key)) {
                base[i] = nullopt;
            } else if (isGlobalKey(key)) {
                auto found = exactGlobals.find(globalIndex(key));
                if (found != exactGlobals.end()) base[i] = found->second;
            } else if (key >= 0 && key < static_cast<int>(env.size())) {
                base[i] = env[static_cast<size_t>(key)];
            }
        }
        base.back() = 1;
        return base;
    }

    bool buildPeriodicClosed(const Stmt *loop, const ExactEnv &env,
                             const Model &model, CountedLoop &counted,
                             Matrix &closed,
                             bool requireProfitability = true) const {
        static constexpr uint64_t kMaxPeriodicPhases = 4096;

        optional<int32_t> step;
        if (!findUnconditionalStep(loop->body.get(), counted.inductionKey, env, step) ||
            !step || *step == 0 || counted.start < 0) {
            return false;
        }
        counted.step = *step;
        auto trip = tripCount(counted);
        if (!trip) return false;
        counted.trips = trip->first;
        counted.finalValue = trip->second;
        if (counted.trips == 0) {
            closed = identityMatrix(static_cast<int>(model.keys.size()) + 1);
            return true;
        }
        int64_t lastPhaseValue = static_cast<int64_t>(counted.start) +
            static_cast<int64_t>(counted.trips - 1) * counted.step;
        if (lastPhaseValue < 0 ||
            lastPhaseValue > numeric_limits<int32_t>::max()) {
            return false;
        }

        vector<int32_t> moduli;
        bool sawPeriodic = false;
        unordered_set<int> phaseExactKeys;
        if (!collectPeriodicStructure(loop->body.get(), counted.inductionKey, env,
                                      model, phaseExactKeys, moduli, sawPeriodic) ||
            !sawPeriodic) {
            return false;
        }

        uint64_t period = 1;
        uint64_t stepMagnitude = counted.step < 0
            ? static_cast<uint64_t>(-static_cast<int64_t>(counted.step))
            : static_cast<uint64_t>(counted.step);
        for (int32_t divisor : moduli) {
            uint64_t modulus = static_cast<uint64_t>(divisor);
            uint64_t phaseCount = modulus /
                std::gcd(modulus, stepMagnitude % modulus);
            uint64_t common = std::gcd(period, phaseCount);
            uint64_t factor = phaseCount / common;
            if (period > kMaxPeriodicPhases / factor) return false;
            period *= factor;
        }
        if (period == 0 || period > kMaxPeriodicPhases ||
            (requireProfitability &&
             counted.trips < max<uint64_t>(8, period * 2))) {
            return false;
        }

        const int states = static_cast<int>(model.keys.size());
        const int dimension = states + 1;
        const int inductionIndex = model.index.at(counted.inductionKey);
        vector<Matrix> phases;
        uint64_t phasesToBuild = min(period, counted.trips);
        phases.reserve(static_cast<size_t>(phasesToBuild));
        Matrix periodTransform = identityMatrix(dimension);

        for (uint64_t phase = 0; phase < phasesToBuild; ++phase) {
            int64_t phaseValue = static_cast<int64_t>(counted.start) +
                                 static_cast<int64_t>(phase) * counted.step;
            if (phaseValue < 0 || phaseValue > numeric_limits<int32_t>::max()) return false;
            auto base = periodicBase(model, env, counted.inductionKey,
                                     static_cast<int32_t>(phaseValue));
            Matrix transform = identityMatrix(dimension);
            if (!applyBodyTransform(loop->body.get(), model, transform, &base)) return false;

            const Row &inductionRow = transform[static_cast<size_t>(inductionIndex)];
            for (int i = 0; i < states; ++i) {
                uint32_t expected = i == inductionIndex ? 1u : 0u;
                if (inductionRow[static_cast<size_t>(i)] != expected) return false;
            }
            if (static_cast<int32_t>(inductionRow.back()) != counted.step) return false;
            periodTransform = multiply(transform, periodTransform);
            phases.push_back(std::move(transform));
        }

        if (counted.trips < period) {
            closed = identityMatrix(dimension);
            for (const Matrix &phase : phases) {
                closed = multiply(phase, closed);
            }
            return true;
        }

        uint64_t wholePeriods = counted.trips / period;
        uint64_t remainder = counted.trips % period;
        closed = power(std::move(periodTransform), wholePeriods);
        for (uint64_t phase = 0; phase < remainder; ++phase) {
            closed = multiply(phases[static_cast<size_t>(phase)], closed);
        }
        return true;
    }

    static unique_ptr<Expr> varExpr(const string &name) {
        auto expr = make_unique<Expr>();
        expr->kind = Expr::Kind::Var;
        expr->name = name;
        return expr;
    }

    static unique_ptr<Expr> binaryExpr(string op, unique_ptr<Expr> lhs,
                                       unique_ptr<Expr> rhs) {
        auto expr = make_unique<Expr>();
        expr->kind = Expr::Kind::Binary;
        expr->op = std::move(op);
        expr->lhs = std::move(lhs);
        expr->rhs = std::move(rhs);
        return expr;
    }

    static unique_ptr<Stmt> declStmt(const string &name, unique_ptr<Expr> init) {
        auto stmt = make_unique<Stmt>();
        stmt->kind = Stmt::Kind::DeclStmt;
        stmt->decl = make_unique<Decl>();
        stmt->decl->name = name;
        stmt->decl->init = std::move(init);
        return stmt;
    }

    static unique_ptr<Stmt> assignStmt(const string &name, unique_ptr<Expr> value) {
        auto stmt = make_unique<Stmt>();
        stmt->kind = Stmt::Kind::Assign;
        stmt->name = name;
        stmt->expr = std::move(value);
        return stmt;
    }

    static unique_ptr<Expr> finalExpr(const Row &row, const Model &model,
                                      const unordered_map<int, string> &temporaries) {
        unique_ptr<Expr> sum;
        auto append = [&](unique_ptr<Expr> term) {
            if (!sum) sum = std::move(term);
            else sum = binaryExpr("+", std::move(sum), std::move(term));
        };

        for (size_t i = 0; i < model.keys.size(); ++i) {
            uint32_t coefficient = row[i];
            if (coefficient == 0) continue;
            int key = model.keys[i];
            string source = model.names.at(key);
            auto temp = temporaries.find(key);
            if (temp != temporaries.end()) source = temp->second;
            unique_ptr<Expr> term = varExpr(source);
            if (coefficient != 1) {
                term = binaryExpr("*", std::move(term),
                                  makeNumberExpr(static_cast<int32_t>(coefficient)));
            }
            append(std::move(term));
        }
        uint32_t constant = row.back();
        if (constant != 0 || !sum) append(makeNumberExpr(static_cast<int32_t>(constant)));
        return sum;
    }

    bool rewriteClosedLoop(unique_ptr<Stmt> &stmt, const Model &model,
                           const Matrix &closed,
                           unordered_set<int> &modifiedLocals) {
        vector<int> persistent;
        for (int key : model.keys) {
            if (model.modified.count(key) && !model.transient.count(key)) {
                persistent.push_back(key);
            }
        }
        if (persistent.empty()) return false;
        for (int key : persistent) {
            const Row &row = closed[static_cast<size_t>(model.index.at(key))];
            for (int transient : model.transient) {
                if (row[static_cast<size_t>(model.index.at(transient))] != 0) return false;
            }
        }

        unordered_map<int, string> temporaries;
        auto wrapper = make_unique<Stmt>();
        wrapper->kind = Stmt::Kind::Block;
        for (int key : persistent) {
            string temporary = freshName();
            temporaries[key] = temporary;
            wrapper->stmts.push_back(declStmt(temporary, varExpr(model.names.at(key))));
            if (!isGlobalKey(key)) modifiedLocals.insert(key);
        }

        auto guarded = make_unique<Stmt>();
        guarded->kind = Stmt::Kind::If;
        guarded->expr = std::move(stmt->expr);
        guarded->thenStmt = make_unique<Stmt>();
        guarded->thenStmt->kind = Stmt::Kind::Block;
        for (int key : persistent) {
            const Row &row = closed[static_cast<size_t>(model.index.at(key))];
            guarded->thenStmt->stmts.push_back(
                assignStmt(model.names.at(key), finalExpr(row, model, temporaries)));
        }
        wrapper->stmts.push_back(std::move(guarded));
        stmt = std::move(wrapper);
        changed = true;
        return true;
    }

    // Model each additive loop delta as P(k) over the iteration index.  A
    // fixed-size forward-difference table sums P(0)..P(n-1) without walking
    // the ToyC loop; all coefficient arithmetic is in the RV32 wraparound ring.
    static Polynomial polynomialConstant(uint32_t value) {
        Polynomial out;
        out.coefficients[0] = value;
        return out;
    }

    static Polynomial polynomialAdd(Polynomial lhs, const Polynomial &rhs) {
        for (int degree = 0; degree < kPolynomialTerms; ++degree) {
            lhs.coefficients[static_cast<size_t>(degree)] +=
                rhs.coefficients[static_cast<size_t>(degree)];
        }
        return lhs;
    }

    static Polynomial polynomialNegate(Polynomial value) {
        for (uint32_t &coefficient : value.coefficients) {
            coefficient = 0u - coefficient;
        }
        return value;
    }

    static bool polynomialMultiply(const Polynomial &lhs,
                                   const Polynomial &rhs,
                                   Polynomial &out) {
        out = Polynomial{};
        for (int lhsDegree = 0; lhsDegree < kPolynomialTerms; ++lhsDegree) {
            uint32_t lhsValue = lhs.coefficients[static_cast<size_t>(lhsDegree)];
            if (lhsValue == 0) continue;
            for (int rhsDegree = 0; rhsDegree < kPolynomialTerms; ++rhsDegree) {
                uint32_t rhsValue = rhs.coefficients[static_cast<size_t>(rhsDegree)];
                if (rhsValue == 0) continue;
                if (lhsDegree + rhsDegree >= kPolynomialTerms) return false;
                size_t degree = static_cast<size_t>(lhsDegree + rhsDegree);
                out.coefficients[degree] += static_cast<uint32_t>(
                    static_cast<uint64_t>(lhsValue) * rhsValue);
            }
        }
        return true;
    }

    static uint32_t multiplyMod32(uint64_t lhs, uint64_t rhs) {
        return static_cast<uint32_t>(static_cast<uint32_t>(lhs) *
                                     static_cast<uint64_t>(static_cast<uint32_t>(rhs)));
    }

    static uint32_t binomialMod32(uint64_t n, int choose) {
        if (choose < 0 || n < static_cast<uint64_t>(choose)) return 0;
        if (choose == 0) return 1;
        vector<uint64_t> factors;
        factors.reserve(static_cast<size_t>(choose));
        for (int i = 0; i < choose; ++i) {
            factors.push_back(n - static_cast<uint64_t>(i));
        }
        for (int divisor = 2; divisor <= choose; ++divisor) {
            int remaining = divisor;
            for (int prime = 2; prime <= remaining; ++prime) {
                while (remaining % prime == 0) {
                    auto factor = find_if(
                        factors.begin(), factors.end(),
                        [prime](uint64_t value) {
                            return value % static_cast<uint64_t>(prime) == 0;
                        });
                    if (factor == factors.end()) return 0;
                    *factor /= static_cast<uint64_t>(prime);
                    remaining /= prime;
                }
            }
        }
        uint32_t out = 1;
        for (uint64_t factor : factors) out = multiplyMod32(out, factor);
        return out;
    }

    static uint32_t sumPolynomial(const Polynomial &polynomial,
                                  uint64_t count) {
        array<uint32_t, kPolynomialTerms> differences{};
        for (int sample = 0; sample < kPolynomialTerms; ++sample) {
            uint32_t value = 0;
            for (int degree = kPolynomialTerms - 1; degree >= 0; --degree) {
                value = multiplyMod32(value, static_cast<uint32_t>(sample));
                value += polynomial.coefficients[static_cast<size_t>(degree)];
            }
            differences[static_cast<size_t>(sample)] = value;
        }

        uint32_t out = 0;
        for (int order = 0; order < kPolynomialTerms; ++order) {
            out += multiplyMod32(
                differences[0], binomialMod32(count, order + 1));
            for (int i = 0; i + order + 1 < kPolynomialTerms; ++i) {
                differences[static_cast<size_t>(i)] =
                    differences[static_cast<size_t>(i + 1)] -
                    differences[static_cast<size_t>(i)];
            }
        }
        return out;
    }

    optional<int32_t> polynomialInvariantValue(
        const Expr *expr, const ExactEnv &env,
        const unordered_set<int> &changing) const {
        unordered_set<int> keys;
        collectExprKeys(expr, keys);
        for (int key : keys) {
            if (changing.count(key)) return nullopt;
        }
        return evalExact(expr, env);
    }

    bool polynomialExpr(const Expr *expr, int inductionKey,
                        int32_t inductionBase, int32_t step,
                        const unordered_set<int> &changing,
                        const unordered_map<int, Polynomial> &aliases,
                        const ExactEnv &env, Polynomial &out) const {
        if (!expr) return false;
        if (auto invariant = polynomialInvariantValue(expr, env, changing)) {
            out = polynomialConstant(static_cast<uint32_t>(*invariant));
            return true;
        }
        switch (expr->kind) {
            case Expr::Kind::Number:
                out = polynomialConstant(static_cast<uint32_t>(wrap32(expr->value)));
                return true;
            case Expr::Kind::Var: {
                int key = exprKey(expr);
                auto alias = aliases.find(key);
                if (alias != aliases.end()) {
                    out = alias->second;
                    return true;
                }
                if (key != inductionKey) return false;
                out = Polynomial{};
                out.coefficients[0] = static_cast<uint32_t>(inductionBase);
                out.coefficients[1] = static_cast<uint32_t>(step);
                return true;
            }
            case Expr::Kind::Call:
                return false;
            case Expr::Kind::Unary: {
                if (!polynomialExpr(expr->lhs.get(), inductionKey,
                                    inductionBase, step, changing, aliases,
                                    env, out)) {
                    return false;
                }
                if (expr->op == "+") return true;
                if (expr->op == "-") {
                    out = polynomialNegate(out);
                    return true;
                }
                return false;
            }
            case Expr::Kind::Binary: {
                if (expr->op != "+" && expr->op != "-" && expr->op != "*") {
                    return false;
                }
                Polynomial lhs;
                Polynomial rhs;
                if (!polynomialExpr(expr->lhs.get(), inductionKey,
                                    inductionBase, step, changing, aliases,
                                    env, lhs) ||
                    !polynomialExpr(expr->rhs.get(), inductionKey,
                                    inductionBase, step, changing, aliases,
                                    env, rhs)) {
                    return false;
                }
                if (expr->op == "+") {
                    out = polynomialAdd(lhs, rhs);
                    return true;
                }
                if (expr->op == "-") {
                    out = polynomialAdd(lhs, polynomialNegate(rhs));
                    return true;
                }
                return polynomialMultiply(lhs, rhs, out);
            }
        }
        return false;
    }

    bool polynomialInductionDelta(
        const Expr *expr, int inductionKey, const ExactEnv &env,
        const unordered_set<int> &changing, int32_t &delta) const {
        if (!expr) return false;
        if (expr->kind == Expr::Kind::Var && exprKey(expr) == inductionKey) {
            delta = 0;
            return true;
        }
        if (expr->kind == Expr::Kind::Unary && expr->op == "+") {
            return polynomialInductionDelta(expr->lhs.get(), inductionKey,
                                            env, changing, delta);
        }
        if (expr->kind != Expr::Kind::Binary) return false;

        int32_t nested = 0;
        if ((expr->op == "+" || expr->op == "-") &&
            polynomialInductionDelta(expr->lhs.get(), inductionKey, env,
                                     changing, nested)) {
            auto constant = polynomialInvariantValue(expr->rhs.get(), env, changing);
            if (!constant) return false;
            delta = expr->op == "+" ? add32(nested, *constant)
                                      : sub32(nested, *constant);
            return true;
        }
        if (expr->op == "+" &&
            polynomialInductionDelta(expr->rhs.get(), inductionKey, env,
                                     changing, nested)) {
            auto constant = polynomialInvariantValue(expr->lhs.get(), env, changing);
            if (!constant) return false;
            delta = add32(*constant, nested);
            return true;
        }
        return false;
    }

    static bool expressionContainsKey(const Expr *expr, int key) {
        if (!expr) return false;
        if (expr->kind == Expr::Kind::Var) return exprKey(expr) == key;
        if (expressionContainsKey(expr->lhs.get(), key) ||
            expressionContainsKey(expr->rhs.get(), key)) {
            return true;
        }
        for (auto &arg : expr->args) {
            if (expressionContainsKey(arg.get(), key)) return true;
        }
        return false;
    }

    static bool collectAccumulatorTerms(
        const Expr *expr, int key, int sign, int &coefficient,
        vector<pair<int, const Expr *>> &terms) {
        if (!expr) return false;
        if (expr->kind == Expr::Kind::Var && exprKey(expr) == key) {
            coefficient += sign;
            return coefficient >= -1 && coefficient <= 1;
        }
        if (expr->kind == Expr::Kind::Binary && expr->op == "+") {
            return collectAccumulatorTerms(expr->lhs.get(), key, sign,
                                           coefficient, terms) &&
                   collectAccumulatorTerms(expr->rhs.get(), key, sign,
                                           coefficient, terms);
        }
        if (expr->kind == Expr::Kind::Binary && expr->op == "-") {
            return collectAccumulatorTerms(expr->lhs.get(), key, sign,
                                           coefficient, terms) &&
                   collectAccumulatorTerms(expr->rhs.get(), key, -sign,
                                           coefficient, terms);
        }
        if (expressionContainsKey(expr, key)) return false;
        terms.push_back({sign, expr});
        return true;
    }

    static bool flattenPolynomialBody(const Stmt *stmt,
                                      vector<const Stmt *> &flat) {
        if (!stmt) return true;
        if (stmt->kind == Stmt::Kind::Block) {
            for (auto &child : stmt->stmts) {
                if (!flattenPolynomialBody(child.get(), flat)) return false;
            }
            return true;
        }
        switch (stmt->kind) {
            case Stmt::Kind::Empty:
                return true;
            case Stmt::Kind::ExprStmt:
                if (exprHasCallLocal(stmt->expr.get())) return false;
                flat.push_back(stmt);
                return true;
            case Stmt::Kind::Assign:
                if (stmt->fastDeadStore) return true;
                if (assignKey(stmt) < 0 || !stmt->expr ||
                    exprHasCallLocal(stmt->expr.get())) {
                    return false;
                }
                flat.push_back(stmt);
                return true;
            case Stmt::Kind::DeclStmt:
                if (stmt->fastDeadStore) return true;
                if (!stmt->decl || stmt->decl->fastSlot < 0 ||
                    !stmt->decl->init || exprHasCallLocal(stmt->decl->init.get())) {
                    return false;
                }
                flat.push_back(stmt);
                return true;
            case Stmt::Kind::Block:
            case Stmt::Kind::If:
            case Stmt::Kind::While:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
            case Stmt::Kind::Return:
                return false;
        }
        return false;
    }

    static unique_ptr<Expr> cloneExprPlain(const Expr *expr) {
        static const unordered_map<string, const Expr *> noSubstitutions;
        return cloneExprSubstGeneric(expr, noSubstitutions);
    }

    static unique_ptr<Expr> addSymbolicExpr(unique_ptr<Expr> lhs,
                                            unique_ptr<Expr> rhs) {
        if (!lhs) return rhs;
        if (!rhs) return lhs;
        return binaryExpr("+", std::move(lhs), std::move(rhs));
    }

    static unique_ptr<Expr> negateSymbolicExpr(unique_ptr<Expr> value) {
        if (!value) return nullptr;
        return binaryExpr("-", makeNumberExpr(0), std::move(value));
    }

    static unique_ptr<Expr> multiplySymbolicExpr(unique_ptr<Expr> lhs,
                                                 unique_ptr<Expr> rhs) {
        if (!lhs || !rhs) return nullptr;
        if (auto value = foldConstExpr(lhs.get())) {
            if (*value == 0) return nullptr;
            if (*value == 1) return rhs;
        }
        if (auto value = foldConstExpr(rhs.get())) {
            if (*value == 0) return nullptr;
            if (*value == 1) return lhs;
        }
        return binaryExpr("*", std::move(lhs), std::move(rhs));
    }

    static SymbolicAffine cloneSymbolicAffine(const SymbolicAffine &value) {
        return SymbolicAffine{
            cloneExprPlain(value.constant.get()),
            cloneExprPlain(value.linear.get()),
        };
    }

    static SymbolicAffine addSymbolicAffine(SymbolicAffine lhs,
                                             SymbolicAffine rhs) {
        lhs.constant = addSymbolicExpr(std::move(lhs.constant),
                                       std::move(rhs.constant));
        lhs.linear = addSymbolicExpr(std::move(lhs.linear),
                                     std::move(rhs.linear));
        return lhs;
    }

    static SymbolicAffine negateSymbolicAffine(SymbolicAffine value) {
        value.constant = negateSymbolicExpr(std::move(value.constant));
        value.linear = negateSymbolicExpr(std::move(value.linear));
        return value;
    }

    static SymbolicAffine scaleSymbolicAffine(SymbolicAffine value,
                                               const Expr *factor) {
        value.constant = multiplySymbolicExpr(
            std::move(value.constant), cloneExprPlain(factor));
        value.linear = multiplySymbolicExpr(
            std::move(value.linear), cloneExprPlain(factor));
        return value;
    }

    static bool runtimeInvariantExpr(const Expr *expr,
                                     const unordered_set<int> &changing) {
        if (!expr || exprHasCallLocal(expr)) return false;
        unordered_set<int> keys;
        collectExprKeys(expr, keys);
        for (int key : keys) {
            if (changing.count(key)) return false;
        }
        return true;
    }

    bool symbolicAffineExpr(
        const Expr *expr, int inductionKey,
        const unordered_set<int> &changing,
        const unordered_map<int, SymbolicAffine> &aliases,
        SymbolicAffine &out) const {
        if (!expr) return false;
        if (runtimeInvariantExpr(expr, changing)) {
            out = SymbolicAffine{cloneExprPlain(expr), nullptr};
            return true;
        }
        switch (expr->kind) {
            case Expr::Kind::Number:
                out = SymbolicAffine{cloneExprPlain(expr), nullptr};
                return true;
            case Expr::Kind::Var: {
                int key = exprKey(expr);
                auto alias = aliases.find(key);
                if (alias != aliases.end()) {
                    out = cloneSymbolicAffine(alias->second);
                    return true;
                }
                if (key != inductionKey) return false;
                out = SymbolicAffine{nullptr, makeNumberExpr(1)};
                return true;
            }
            case Expr::Kind::Call:
                return false;
            case Expr::Kind::Unary:
                if (!symbolicAffineExpr(expr->lhs.get(), inductionKey,
                                        changing, aliases, out)) {
                    return false;
                }
                if (expr->op == "+") return true;
                if (expr->op == "-") {
                    out = negateSymbolicAffine(std::move(out));
                    return true;
                }
                return false;
            case Expr::Kind::Binary: {
                if (expr->op == "+" || expr->op == "-") {
                    SymbolicAffine lhs;
                    SymbolicAffine rhs;
                    if (!symbolicAffineExpr(expr->lhs.get(), inductionKey,
                                            changing, aliases, lhs) ||
                        !symbolicAffineExpr(expr->rhs.get(), inductionKey,
                                            changing, aliases, rhs)) {
                        return false;
                    }
                    if (expr->op == "-") {
                        rhs = negateSymbolicAffine(std::move(rhs));
                    }
                    out = addSymbolicAffine(std::move(lhs), std::move(rhs));
                    return true;
                }
                if (expr->op != "*") return false;
                if (runtimeInvariantExpr(expr->lhs.get(), changing)) {
                    if (!symbolicAffineExpr(expr->rhs.get(), inductionKey,
                                            changing, aliases, out)) {
                        return false;
                    }
                    out = scaleSymbolicAffine(std::move(out), expr->lhs.get());
                    return true;
                }
                if (runtimeInvariantExpr(expr->rhs.get(), changing)) {
                    if (!symbolicAffineExpr(expr->lhs.get(), inductionKey,
                                            changing, aliases, out)) {
                        return false;
                    }
                    out = scaleSymbolicAffine(std::move(out), expr->rhs.get());
                    return true;
                }
                return false;
            }
        }
        return false;
    }

    bool extractDynamicUnitCondition(const Expr *condition,
                                     const ExactEnv &env,
                                     CountedLoop &loop) const {
        if (!condition || condition->kind != Expr::Kind::Binary) return false;
        const Expr *induction = nullptr;
        if (condition->op == "<" && condition->lhs &&
            condition->lhs->kind == Expr::Kind::Var &&
            !condition->lhs->fastGlobal) {
            induction = condition->lhs.get();
            loop.boundExpr = condition->rhs.get();
        } else if (condition->op == ">" && condition->rhs &&
                   condition->rhs->kind == Expr::Kind::Var &&
                   !condition->rhs->fastGlobal) {
            induction = condition->rhs.get();
            loop.boundExpr = condition->lhs.get();
        } else {
            return false;
        }
        auto start = evalExact(induction, env);
        if (!start || *start != 0 || evalExact(loop.boundExpr, env) ||
            exprHasCallLocal(loop.boundExpr)) {
            return false;
        }
        loop.inductionKey = exprKey(induction);
        loop.inductionName = induction->name;
        loop.start = 0;
        loop.step = 1;
        loop.relation = "<";
        return loop.inductionKey >= 0;
    }

    bool trySummarizeDynamicUnitLoop(
        unique_ptr<Stmt> &stmt, const ExactEnv &env,
        unordered_set<int> &modifiedLocals) {
        CountedLoop loop;
        if (!stmt || stmt->kind != Stmt::Kind::While || !stmt->expr ||
            !extractDynamicUnitCondition(stmt->expr.get(), env, loop)) {
            return false;
        }

        vector<const Stmt *> flat;
        if (!flattenPolynomialBody(stmt->body.get(), flat) || flat.empty()) {
            return false;
        }
        unordered_set<int> changing;
        unordered_set<int> transient;
        unordered_map<int, string> names;
        names[loop.inductionKey] = loop.inductionName;
        for (const Stmt *item : flat) {
            if (item->kind == Stmt::Kind::Assign) {
                int key = assignKey(item);
                if (key < 0 ||
                    (isGlobalKey(key) &&
                     constGlobals.count(globalIndex(key)))) {
                    return false;
                }
                changing.insert(key);
                names[key] = item->name;
            } else if (item->kind == Stmt::Kind::DeclStmt) {
                int key = item->decl->fastSlot;
                changing.insert(key);
                transient.insert(key);
                names[key] = item->decl->name;
            }
        }
        if (!changing.count(loop.inductionKey)) return false;

        unordered_set<int> boundKeys;
        collectExprKeys(loop.boundExpr, boundKeys);
        for (int key : changing) {
            if (boundKeys.count(key)) return false;
        }

        int inductionUpdates = 0;
        int lastEffect = -1;
        for (int index = 0; index < static_cast<int>(flat.size()); ++index) {
            const Stmt *item = flat[static_cast<size_t>(index)];
            if (item->kind == Stmt::Kind::Assign ||
                item->kind == Stmt::Kind::DeclStmt) {
                lastEffect = index;
            }
            if (item->kind != Stmt::Kind::Assign ||
                assignKey(item) != loop.inductionKey) {
                continue;
            }
            int32_t delta = 0;
            if (!polynomialInductionDelta(item->expr.get(),
                                          loop.inductionKey, env,
                                          changing, delta) ||
                delta != 1) {
                return false;
            }
            ++inductionUpdates;
            if (lastEffect != index) return false;
        }
        if (inductionUpdates != 1 || lastEffect < 0 ||
            flat[static_cast<size_t>(lastEffect)]->kind != Stmt::Kind::Assign ||
            assignKey(flat[static_cast<size_t>(lastEffect)]) !=
                loop.inductionKey) {
            return false;
        }

        unordered_map<int, SymbolicAffine> aliases;
        unordered_map<int, SymbolicAffine> deltas;
        bool sawAccumulator = false;
        for (int index = 0; index < lastEffect; ++index) {
            const Stmt *item = flat[static_cast<size_t>(index)];
            if (item->kind == Stmt::Kind::Empty ||
                item->kind == Stmt::Kind::ExprStmt) {
                continue;
            }
            if (item->kind == Stmt::Kind::DeclStmt) {
                SymbolicAffine value;
                if (!symbolicAffineExpr(item->decl->init.get(),
                                        loop.inductionKey, changing,
                                        aliases, value)) {
                    return false;
                }
                aliases[item->decl->fastSlot] = std::move(value);
                continue;
            }

            int key = assignKey(item);
            if (key == loop.inductionKey) return false;
            if (transient.count(key)) {
                SymbolicAffine value;
                if (!symbolicAffineExpr(item->expr.get(), loop.inductionKey,
                                        changing, aliases, value)) {
                    return false;
                }
                aliases[key] = std::move(value);
                continue;
            }

            int coefficient = 0;
            vector<pair<int, const Expr *>> terms;
            if (!collectAccumulatorTerms(item->expr.get(), key, 1,
                                         coefficient, terms) ||
                coefficient != 1) {
                return false;
            }
            for (const auto &[sign, termExpr] : terms) {
                SymbolicAffine term;
                if (!symbolicAffineExpr(termExpr, loop.inductionKey,
                                        changing, aliases, term)) {
                    return false;
                }
                if (sign < 0) {
                    term = negateSymbolicAffine(std::move(term));
                }
                auto found = deltas.find(key);
                if (found == deltas.end()) {
                    deltas.emplace(key, std::move(term));
                } else {
                    found->second = addSymbolicAffine(
                        std::move(found->second), std::move(term));
                }
            }
            aliases.erase(key);
            sawAccumulator = true;
        }
        if (!sawAccumulator) return false;

        string countName = freshName();
        string halfName = freshName();
        string triangularName = freshName();
        auto wrapper = make_unique<Stmt>();
        wrapper->kind = Stmt::Kind::Block;
        wrapper->stmts.push_back(
            declStmt(countName, cloneExprPlain(loop.boundExpr)));

        auto guarded = make_unique<Stmt>();
        guarded->kind = Stmt::Kind::If;
        guarded->expr = binaryExpr("<", varExpr(loop.inductionName),
                                   varExpr(countName));
        guarded->thenStmt = make_unique<Stmt>();
        guarded->thenStmt->kind = Stmt::Kind::Block;
        guarded->thenStmt->stmts.push_back(declStmt(
            halfName,
            binaryExpr("/", varExpr(countName), makeNumberExpr(2))));
        auto odd = binaryExpr("%", varExpr(countName), makeNumberExpr(2));
        auto otherFactor = binaryExpr(
            "+", binaryExpr("-", varExpr(countName), makeNumberExpr(1)),
            std::move(odd));
        guarded->thenStmt->stmts.push_back(declStmt(
            triangularName,
            binaryExpr("*", varExpr(halfName), std::move(otherFactor))));

        vector<int> accumulatorKeys;
        accumulatorKeys.reserve(deltas.size());
        for (const auto &[key, unused] : deltas) {
            (void)unused;
            accumulatorKeys.push_back(key);
        }
        sort(accumulatorKeys.begin(), accumulatorKeys.end());
        for (int key : accumulatorKeys) {
            SymbolicAffine delta = std::move(deltas.at(key));
            unique_ptr<Expr> total = multiplySymbolicExpr(
                std::move(delta.constant), varExpr(countName));
            total = addSymbolicExpr(
                std::move(total),
                multiplySymbolicExpr(std::move(delta.linear),
                                     varExpr(triangularName)));
            if (!total) continue;
            guarded->thenStmt->stmts.push_back(assignStmt(
                names.at(key),
                binaryExpr("+", varExpr(names.at(key)), std::move(total))));
        }
        guarded->thenStmt->stmts.push_back(
            assignStmt(loop.inductionName, varExpr(countName)));
        wrapper->stmts.push_back(std::move(guarded));
        stmt = std::move(wrapper);
        for (int key : changing) {
            if (!isGlobalKey(key)) modifiedLocals.insert(key);
        }
        changed = true;
        return true;
    }

    bool trySummarizePolynomial(unique_ptr<Stmt> &stmt, const ExactEnv &env,
                                CountedLoop &counted,
                                unordered_set<int> &modifiedLocals) {
        if (!stmt || stmt->kind != Stmt::Kind::While || !stmt->expr ||
            exprHasCallLocal(stmt->expr.get()) ||
            !extractCondition(stmt->expr.get(), env, counted)) {
            return false;
        }

        vector<const Stmt *> flat;
        if (!flattenPolynomialBody(stmt->body.get(), flat) || flat.empty()) {
            return false;
        }

        unordered_set<int> changing;
        unordered_set<int> transient;
        unordered_map<int, string> names;
        names[counted.inductionKey] = counted.inductionName;
        for (const Stmt *item : flat) {
            if (item->kind == Stmt::Kind::Assign) {
                int key = assignKey(item);
                changing.insert(key);
                names[key] = item->name;
                if (isGlobalKey(key) && constGlobals.count(globalIndex(key))) {
                    return false;
                }
            } else if (item->kind == Stmt::Kind::DeclStmt) {
                int key = item->decl->fastSlot;
                changing.insert(key);
                transient.insert(key);
                names[key] = item->decl->name;
            }
        }
        if (!changing.count(counted.inductionKey)) return false;

        unordered_set<int> boundKeys;
        collectExprKeys(counted.boundExpr, boundKeys);
        for (int key : changing) {
            if (boundKeys.count(key)) return false;
        }

        int32_t step = 0;
        int inductionUpdates = 0;
        for (const Stmt *item : flat) {
            if (item->kind != Stmt::Kind::Assign ||
                assignKey(item) != counted.inductionKey) {
                continue;
            }
            int32_t delta = 0;
            if (!polynomialInductionDelta(item->expr.get(), counted.inductionKey,
                                          env, changing, delta)) {
                return false;
            }
            step = add32(step, delta);
            ++inductionUpdates;
        }
        if (inductionUpdates == 0 || step == 0) return false;
        counted.step = step;
        auto trip = tripCount(counted);
        if (!trip) return false;
        counted.trips = trip->first;
        counted.finalValue = trip->second;
        if (counted.trips == 0) {
            auto empty = make_unique<Stmt>();
            empty->kind = Stmt::Kind::Empty;
            stmt = std::move(empty);
            changed = true;
            return true;
        }
        if (counted.trips < 8) return false;

        unordered_map<int, Polynomial> aliases;
        unordered_map<int, Polynomial> deltas;
        int32_t inductionOffset = 0;
        bool sawAccumulator = false;
        for (const Stmt *item : flat) {
            if (item->kind == Stmt::Kind::Empty ||
                item->kind == Stmt::Kind::ExprStmt) {
                continue;
            }
            int32_t inductionBase = add32(counted.start, inductionOffset);
            if (item->kind == Stmt::Kind::DeclStmt) {
                Polynomial value;
                if (!polynomialExpr(item->decl->init.get(), counted.inductionKey,
                                    inductionBase, step, changing, aliases,
                                    env, value)) {
                    return false;
                }
                aliases[item->decl->fastSlot] = value;
                continue;
            }

            int key = assignKey(item);
            if (key == counted.inductionKey) {
                int32_t delta = 0;
                if (!polynomialInductionDelta(item->expr.get(), counted.inductionKey,
                                              env, changing, delta)) {
                    return false;
                }
                inductionOffset = add32(inductionOffset, delta);
                aliases.erase(key);
                continue;
            }
            if (transient.count(key)) {
                Polynomial value;
                if (!polynomialExpr(item->expr.get(), counted.inductionKey,
                                    inductionBase, step, changing, aliases,
                                    env, value)) {
                    return false;
                }
                aliases[key] = value;
                continue;
            }

            int coefficient = 0;
            vector<pair<int, const Expr *>> terms;
            if (!collectAccumulatorTerms(item->expr.get(), key, 1,
                                         coefficient, terms) ||
                coefficient != 1) {
                return false;
            }
            for (const auto &[sign, termExpr] : terms) {
                Polynomial term;
                if (!polynomialExpr(termExpr, counted.inductionKey,
                                    inductionBase, step, changing, aliases,
                                    env, term)) {
                    return false;
                }
                if (sign < 0) term = polynomialNegate(term);
                deltas[key] = polynomialAdd(deltas[key], term);
            }
            aliases.erase(key);
            sawAccumulator = true;
        }
        if (!sawAccumulator || inductionOffset != step) return false;

        auto wrapper = make_unique<Stmt>();
        wrapper->kind = Stmt::Kind::Block;
        auto guarded = make_unique<Stmt>();
        guarded->kind = Stmt::Kind::If;
        guarded->expr = std::move(stmt->expr);
        guarded->thenStmt = make_unique<Stmt>();
        guarded->thenStmt->kind = Stmt::Kind::Block;

        vector<int> accumulatorKeys;
        accumulatorKeys.reserve(deltas.size());
        for (const auto &[key, unused] : deltas) {
            (void)unused;
            accumulatorKeys.push_back(key);
        }
        sort(accumulatorKeys.begin(), accumulatorKeys.end());
        for (int key : accumulatorKeys) {
            uint32_t delta = sumPolynomial(deltas.at(key), counted.trips);
            if (delta != 0) {
                guarded->thenStmt->stmts.push_back(assignStmt(
                    names.at(key),
                    binaryExpr("+", varExpr(names.at(key)),
                               makeNumberExpr(static_cast<int32_t>(delta)))));
            }
        }
        guarded->thenStmt->stmts.push_back(
            assignStmt(counted.inductionName,
                       makeNumberExpr(counted.finalValue)));
        wrapper->stmts.push_back(std::move(guarded));
        stmt = std::move(wrapper);
        for (int key : changing) {
            if (!isGlobalKey(key)) modifiedLocals.insert(key);
        }
        changed = true;
        return true;
    }

    bool trySummarize(unique_ptr<Stmt> &stmt, const ExactEnv &env,
                      CountedLoop &counted, unordered_set<int> &modifiedLocals) {
        if (!stmt || stmt->kind != Stmt::Kind::While || !stmt->expr ||
            exprHasCallLocal(stmt->expr.get())) {
            return false;
        }
        if (!extractCondition(stmt->expr.get(), env, counted)) return false;

        Model model;
        addModelKey(model, counted.inductionKey, counted.inductionName);
        if (!collectBody(stmt->body.get(), model) ||
            model.keys.size() > 32 || !model.modified.count(counted.inductionKey)) {
            return false;
        }
        for (int key : model.modified) {
            if (isGlobalKey(key) && constGlobals.count(globalIndex(key))) return false;
        }

        unordered_set<int> boundKeys;
        Model boundModel;
        if (!collectExprVars(counted.boundExpr, boundModel)) return false;
        for (int key : boundModel.keys) boundKeys.insert(key);
        for (int key : model.modified) {
            if (boundKeys.count(key)) return false;
        }

        const int states = static_cast<int>(model.keys.size());
        const int dimension = states + 1;
        Matrix transform = identityMatrix(dimension);
        Matrix closed;
        if (applyBodyTransform(stmt->body.get(), model, transform)) {
            int inductionIndex = model.index.at(counted.inductionKey);
            const Row &inductionRow = transform[static_cast<size_t>(inductionIndex)];
            for (int i = 0; i < states; ++i) {
                uint32_t expected = i == inductionIndex ? 1u : 0u;
                if (inductionRow[static_cast<size_t>(i)] != expected) return false;
            }
            counted.step = static_cast<int32_t>(inductionRow.back());
            auto trip = tripCount(counted);
            if (!trip) return false;
            counted.trips = trip->first;
            counted.finalValue = trip->second;
            if (counted.trips == 0) {
                auto empty = make_unique<Stmt>();
                empty->kind = Stmt::Kind::Empty;
                stmt = std::move(empty);
                changed = true;
                return true;
            }
            if (counted.trips < 8) return false;
            closed = power(std::move(transform), counted.trips);
        } else if (!buildPeriodicClosed(stmt.get(), env, model, counted, closed)) {
            return false;
        }

        return rewriteClosedLoop(stmt, model, closed, modifiedLocals);
    }

    static void collectAssignedLocals(const Stmt *s, unordered_set<int> &slots) {
        if (!s) return;
        if (s->kind == Stmt::Kind::Assign && !s->fastAssignGlobal && s->fastAssignIndex >= 0) {
            slots.insert(s->fastAssignIndex);
        } else if (s->kind == Stmt::Kind::DeclStmt && s->decl && s->decl->fastSlot >= 0) {
            slots.insert(s->decl->fastSlot);
        }
        for (auto &child : s->stmts) collectAssignedLocals(child.get(), slots);
        collectAssignedLocals(s->thenStmt.get(), slots);
        collectAssignedLocals(s->elseStmt.get(), slots);
        collectAssignedLocals(s->body.get(), slots);
    }

    static ExactEnv mergeEnvs(const ExactEnv &lhs, const ExactEnv &rhs) {
        ExactEnv out = lhs;
        for (size_t i = 0; i < out.size(); ++i) {
            if (!lhs[i] || !rhs[i] || *lhs[i] != *rhs[i]) out[i] = nullopt;
        }
        return out;
    }

    void optimizeStmt(unique_ptr<Stmt> &stmt, ExactEnv &env) {
        if (!stmt) return;
        switch (stmt->kind) {
            case Stmt::Kind::Block:
                for (auto &child : stmt->stmts) optimizeStmt(child, env);
                return;
            case Stmt::Kind::Empty:
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
            case Stmt::Kind::Return:
                return;
            case Stmt::Kind::DeclStmt:
                if (stmt->decl && stmt->decl->fastSlot >= 0 &&
                    stmt->decl->fastSlot < static_cast<int>(env.size())) {
                    env[static_cast<size_t>(stmt->decl->fastSlot)] =
                        evalExact(stmt->decl->init.get(), env);
                }
                return;
            case Stmt::Kind::Assign:
                if (!stmt->fastAssignGlobal && stmt->fastAssignIndex >= 0 &&
                    stmt->fastAssignIndex < static_cast<int>(env.size())) {
                    env[static_cast<size_t>(stmt->fastAssignIndex)] = evalExact(stmt->expr.get(), env);
                }
                return;
            case Stmt::Kind::If: {
                auto condition = evalExact(stmt->expr.get(), env);
                if (condition) {
                    if (truthy(*condition)) optimizeStmt(stmt->thenStmt, env);
                    else optimizeStmt(stmt->elseStmt, env);
                    return;
                }
                ExactEnv thenEnv = env;
                ExactEnv elseEnv = env;
                optimizeStmt(stmt->thenStmt, thenEnv);
                optimizeStmt(stmt->elseStmt, elseEnv);
                env = mergeEnvs(thenEnv, elseEnv);
                return;
            }
            case Stmt::Kind::While: {
                CountedLoop counted;
                unordered_set<int> modified;
                if (tryDropDeadLoop(stmt, env, counted, modified)) {
                    for (int slot : modified) {
                        if (slot >= 0 && slot < static_cast<int>(env.size())) {
                            env[static_cast<size_t>(slot)] = nullopt;
                        }
                    }
                    return;
                }
                bool summarized = trySummarize(stmt, env, counted, modified);
                if (!summarized) {
                    counted = CountedLoop{};
                    modified.clear();
                    summarized = trySummarizePolynomial(stmt, env, counted, modified);
                }
                if (summarized) {
                    for (int slot : modified) {
                        if (slot >= 0 && slot < static_cast<int>(env.size())) {
                            env[static_cast<size_t>(slot)] = nullopt;
                        }
                    }
                    if (counted.inductionKey >= 0 && !isGlobalKey(counted.inductionKey) &&
                        counted.inductionKey < static_cast<int>(env.size())) {
                        env[static_cast<size_t>(counted.inductionKey)] = counted.finalValue;
                    }
                    return;
                }
                modified.clear();
                if (trySummarizeDynamicUnitLoop(stmt, env, modified)) {
                    for (int slot : modified) {
                        if (slot >= 0 && slot < static_cast<int>(env.size())) {
                            env[static_cast<size_t>(slot)] = nullopt;
                        }
                    }
                    return;
                }

                unordered_set<int> assigned;
                collectAssignedLocals(stmt->body.get(), assigned);
                ExactEnv bodyEnv = env;
                for (int slot : assigned) {
                    if (slot >= 0 && slot < static_cast<int>(bodyEnv.size())) {
                        bodyEnv[static_cast<size_t>(slot)] = nullopt;
                    }
                }
                optimizeStmt(stmt->body, bodyEnv);
                for (int slot : assigned) {
                    if (slot >= 0 && slot < static_cast<int>(env.size())) {
                        env[static_cast<size_t>(slot)] = nullopt;
                    }
                }
                return;
            }
        }
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
        collectImmutableGlobals();
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
    unordered_map<string, Function *> inlineableFuncs;
    unordered_map<string, Function *> branchInlineableFuncs;
    unordered_map<string, Function *> loopInlineableFuncs;
    unordered_map<string, vector<int>> loopInlineRegisterSlots;
    unordered_set<string> globalNames;
    unordered_set<string> writableGlobalNames;
    unordered_set<string> immutableGlobals;
    vector<unordered_map<string, Symbol>> scopes;
    vector<string> breakLabels;
    vector<string> continueLabels;
    string returnLabel;
    string currentFunctionName;
    string functionBodyLabel;
    vector<string> currentParams;
    vector<string> savedVarRegs;
    vector<string> allocableVarRegs;
    vector<string> inlineArgRegs;
    unordered_map<int, string> slotRegMap;
    vector<pair<int, uint64_t>> rankedSlotCandidates;
    bool useSlotRegMap = false;
    int varRegCap = 0;
    unordered_map<long long, string> constRegMap;
    vector<pair<long long, string>> hoistedConsts;
    struct CachedGlobal {
        string name;
        string label;
        string reg;
        bool written = false;
    };
    vector<CachedGlobal> cachedGlobals;
    unordered_map<string, string> cachedGlobalRegs;
    int labelId = 0;
    int nextSlot = 0;
    int nextVarReg = 0;
    int localBaseOffset = -12;
    int frameSize = 0;
    int loopInlineScratchBaseOffset = 0;
    int loopInlineScratchSlots = 0;
    vector<string> loopInlineHomeRegs;
    bool frameFreeLeaf = false;

    static int alignTo(int x, int a) { return (x + a - 1) / a * a; }
    static bool fits12(long long x) { return x >= -2048 && x <= 2047; }
    static string globalLabel(const string &name) { return ".Lglob_" + name; }

    string newLabel(const string &prefix) {
        return ".L" + prefix + "_" + to_string(labelId++);
    }

    void emit(const string &s) { out << "    " << s << "\n"; }
    void emitLabel(const string &s) { out << s << ":\n"; }

    // Move incoming argument registers into arbitrary leaf homes without
    // clobbering a source that is still needed by a later move.
    void emitParallelMoves(vector<pair<string, string>> moves) {
        moves.erase(remove_if(moves.begin(), moves.end(),
                               [](const auto &m) { return m.first == m.second; }),
                    moves.end());
        while (!moves.empty()) {
            bool emitted = false;
            for (size_t i = 0; i < moves.size(); ++i) {
                const string &dst = moves[i].first;
                bool usedAsSource = false;
                for (const auto &move : moves) {
                    if (move.second == dst) {
                        usedAsSource = true;
                        break;
                    }
                }
                if (usedAsSource) continue;
                emit("mv " + dst + ", " + moves[i].second);
                moves.erase(moves.begin() + static_cast<ptrdiff_t>(i));
                emitted = true;
                break;
            }
            if (emitted) continue;

            // The remaining moves form a cycle. t6 is reserved as a scratch
            // register by this backend and is never a leaf home.
            emit("mv t6, " + moves.front().second);
            moves.front().second = "t6";
        }
    }

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

    enum InlineFlow : unsigned {
        InlineFallthrough = 1u,
        InlineReturn = 2u,
        InlineBreak = 4u,
        InlineContinue = 8u,
    };

    bool loopInlineExprShape(const Expr *e, int localCount, int &nodes) const {
        if (!e) return true;
        if (++nodes > 256 || e->kind == Expr::Kind::Call) return false;
        if (e->kind == Expr::Kind::Var) {
            if (e->fastIndex < 0) return false;
            return e->fastGlobal ? globalNames.count(e->name) != 0
                                 : e->fastIndex < localCount;
        }
        if (!loopInlineExprShape(e->lhs.get(), localCount, nodes) ||
            !loopInlineExprShape(e->rhs.get(), localCount, nodes)) {
            return false;
        }
        for (auto &arg : e->args) {
            if (!loopInlineExprShape(arg.get(), localCount, nodes)) return false;
        }
        return true;
    }

    bool loopInlineStmtShape(const Stmt *s, int loopDepth, int localCount,
                             const unordered_set<int> &mutableSlots, int &nodes,
                             bool &hasLoop, unsigned &flow) const {
        if (!s) {
            flow = InlineFallthrough;
            return true;
        }
        if (++nodes > 256) return false;
        switch (s->kind) {
            case Stmt::Kind::Block: {
                unsigned aggregate = InlineFallthrough;
                for (auto &child : s->stmts) {
                    unsigned childFlow = 0;
                    if (!loopInlineStmtShape(child.get(), loopDepth, localCount,
                                             mutableSlots, nodes, hasLoop, childFlow)) {
                        return false;
                    }
                    if (aggregate & InlineFallthrough) {
                        aggregate = (aggregate & ~InlineFallthrough) | childFlow;
                    }
                }
                flow = aggregate;
                return true;
            }
            case Stmt::Kind::Empty:
                flow = InlineFallthrough;
                return true;
            case Stmt::Kind::ExprStmt:
                if (!loopInlineExprShape(s->expr.get(), localCount, nodes)) return false;
                flow = InlineFallthrough;
                return true;
            case Stmt::Kind::DeclStmt:
                if (!s->decl || !s->decl->init || s->decl->fastSlot < 0 ||
                    s->decl->fastSlot >= localCount ||
                    !loopInlineExprShape(s->decl->init.get(), localCount, nodes)) {
                    return false;
                }
                flow = InlineFallthrough;
                return true;
            case Stmt::Kind::Assign:
                if (s->fastAssignIndex < 0 ||
                    !loopInlineExprShape(s->expr.get(), localCount, nodes)) {
                    return false;
                }
                if (s->fastAssignGlobal) {
                    if (!writableGlobalNames.count(s->name)) return false;
                } else if (s->fastAssignIndex >= localCount ||
                           !mutableSlots.count(s->fastAssignIndex)) {
                    return false;
                }
                flow = InlineFallthrough;
                return true;
            case Stmt::Kind::If: {
                if (!loopInlineExprShape(s->expr.get(), localCount, nodes)) return false;
                unsigned thenFlow = 0;
                unsigned elseFlow = InlineFallthrough;
                if (!loopInlineStmtShape(s->thenStmt.get(), loopDepth, localCount,
                                         mutableSlots, nodes, hasLoop, thenFlow)) {
                    return false;
                }
                if (s->elseStmt &&
                    !loopInlineStmtShape(s->elseStmt.get(), loopDepth, localCount,
                                         mutableSlots, nodes, hasLoop, elseFlow)) {
                    return false;
                }
                flow = thenFlow | elseFlow;
                return true;
            }
            case Stmt::Kind::While: {
                if (!loopInlineExprShape(s->expr.get(), localCount, nodes)) return false;
                hasLoop = true;
                unsigned bodyFlow = 0;
                if (!loopInlineStmtShape(s->body.get(), loopDepth + 1, localCount,
                                         mutableSlots, nodes, hasLoop, bodyFlow)) {
                    return false;
                }
                flow = InlineFallthrough | (bodyFlow & InlineReturn);
                return true;
            }
            case Stmt::Kind::Break:
                if (loopDepth <= 0) return false;
                flow = InlineBreak;
                return true;
            case Stmt::Kind::Continue:
                if (loopDepth <= 0) return false;
                flow = InlineContinue;
                return true;
            case Stmt::Kind::Return:
                if (!s->expr || !loopInlineExprShape(s->expr.get(), localCount, nodes)) {
                    return false;
                }
                flow = InlineReturn;
                return true;
        }
        return false;
    }

    bool branchInlineStmtShape(const Stmt *s, const unordered_set<string> &params,
                               int &nodes, bool &alwaysReturns) const {
        alwaysReturns = false;
        if (!s || ++nodes > 64) return false;
        switch (s->kind) {
            case Stmt::Kind::Block: {
                bool guaranteed = false;
                for (auto &child : s->stmts) {
                    if (guaranteed) break;
                    bool childReturns = false;
                    if (!branchInlineStmtShape(child.get(), params, nodes, childReturns)) {
                        return false;
                    }
                    guaranteed = childReturns;
                }
                alwaysReturns = guaranteed;
                return true;
            }
            case Stmt::Kind::Empty:
                return true;
            case Stmt::Kind::If: {
                if (!s->expr || exprHasCall(s->expr.get()) ||
                    !exprUsesOnlyVars(s->expr.get(), params)) {
                    return false;
                }
                bool thenReturns = false;
                bool elseReturns = false;
                if (!branchInlineStmtShape(s->thenStmt.get(), params, nodes, thenReturns)) {
                    return false;
                }
                if (s->elseStmt &&
                    !branchInlineStmtShape(s->elseStmt.get(), params, nodes, elseReturns)) {
                    return false;
                }
                alwaysReturns = thenReturns && s->elseStmt && elseReturns;
                return true;
            }
            case Stmt::Kind::Return:
                if (!s->expr || exprHasCall(s->expr.get()) ||
                    !exprUsesOnlyVars(s->expr.get(), params)) {
                    return false;
                }
                alwaysReturns = true;
                return true;
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Assign:
            case Stmt::Kind::DeclStmt:
            case Stmt::Kind::While:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return false;
        }
        return false;
    }

    void collectFunctions() {
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Func) {
                Function *function = item.func.get();
                funcs[function->name] = FuncInfo{function->returnsVoid,
                                                 static_cast<int>(function->params.size())};
                const Stmt *body = function->body.get();
                if (!function->returnsVoid && body && body->kind == Stmt::Kind::Block &&
                    body->stmts.size() == 1 && body->stmts[0]->kind == Stmt::Kind::Return &&
                    body->stmts[0]->expr && !exprHasCall(body->stmts[0]->expr.get())) {
                    unordered_set<string> params(function->params.begin(), function->params.end());
                    if (exprUsesOnlyVars(body->stmts[0]->expr.get(), params)) {
                        inlineableFuncs[function->name] = function;
                    }
                }
                if (!function->returnsVoid) {
                    unordered_set<string> params(function->params.begin(), function->params.end());
                    int nodes = 0;
                    bool alwaysReturns = false;
                    if (branchInlineStmtShape(body, params, nodes, alwaysReturns) && alwaysReturns) {
                        branchInlineableFuncs[function->name] = function;
                    }
                    nodes = 0;
                    bool hasLoop = false;
                    unsigned flow = 0;
                    unordered_set<int> mutableSlots;
                    for (int slot = 0; slot < static_cast<int>(function->params.size());
                         ++slot) {
                        mutableSlots.insert(slot);
                    }
                    std::function<void(const Stmt *)> collectMutableSlots =
                        [&](const Stmt *stmt) {
                            if (!stmt) return;
                            if (stmt->kind == Stmt::Kind::DeclStmt && stmt->decl &&
                                !stmt->decl->isConst) {
                                mutableSlots.insert(stmt->decl->fastSlot);
                            }
                            for (auto &child : stmt->stmts) {
                                collectMutableSlots(child.get());
                            }
                            collectMutableSlots(stmt->thenStmt.get());
                            collectMutableSlots(stmt->elseStmt.get());
                            collectMutableSlots(stmt->body.get());
                        };
                    collectMutableSlots(body);
                    if (function->fastLocalCount >=
                            static_cast<int>(function->params.size()) &&
                        function->fastLocalCount <= 32 &&
                        loopInlineStmtShape(body, 0, function->fastLocalCount,
                                            mutableSlots, nodes, hasLoop, flow) &&
                        hasLoop && flow == InlineReturn) {
                        loopInlineableFuncs[function->name] = function;
                        vector<uint64_t> weights(
                            static_cast<size_t>(function->fastLocalCount), 0);
                        collectSlotUses(body, 0, weights);
                        vector<int> ranked(mutableSlots.begin(), mutableSlots.end());
                        sort(ranked.begin(), ranked.end(), [&](int lhs, int rhs) {
                            uint64_t lhsWeight = weights[static_cast<size_t>(lhs)];
                            uint64_t rhsWeight = weights[static_cast<size_t>(rhs)];
                            if (lhsWeight != rhsWeight) return lhsWeight > rhsWeight;
                            return lhs < rhs;
                        });
                        loopInlineRegisterSlots[function->name] = std::move(ranked);
                    }
                }
            }
        }
    }

    void collectImmutableGlobals() {
        unordered_set<string> assignedGlobalNames;
        globalNames.clear();
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Decl) globalNames.insert(item.decl->name);
        }
        for (auto &item : prog.items) {
            if (item.kind == TopItem::Kind::Func) collectGlobalAssignments(item.func->body.get(), globalNames, assignedGlobalNames);
        }
        writableGlobalNames = assignedGlobalNames;
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
                long long v = foldConst(d.init.get());
                globals[d.name] = Symbol{true, v, true, "", 0, ""};
            } else {
                long long v = foldConst(d.init.get());
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
        if (g != globals.end()) {
            auto cached = cachedGlobalRegs.find(name);
            if (cached != cachedGlobalRegs.end() && !g->second.isConst) {
                Symbol symbol = g->second;
                symbol.reg = cached->second;
                return symbol;
            }
            return g->second;
        }
        return nullopt;
    }

    long long foldConst(const Expr *e) {
        switch (e->kind) {
            case Expr::Kind::Number:
                return e->value;
            case Expr::Kind::Var: {
                auto sym = lookup(e->name);
                if (!sym || !sym->isConst) return 0;
                return sym->constValue;
            }
            case Expr::Kind::Unary: {
                long long v = foldConst(e->lhs.get());
                if (e->op == "+") return v;
                if (e->op == "-") return -v;
                if (e->op == "!") return v == 0;
                return 0;
            }
            case Expr::Kind::Binary: {
                if (e->op == "&&") {
                    long long l = foldConst(e->lhs.get());
                    if (!l) return 0;
                    return foldConst(e->rhs.get()) != 0;
                }
                if (e->op == "||") {
                    long long l = foldConst(e->lhs.get());
                    if (l) return 1;
                    return foldConst(e->rhs.get()) != 0;
                }
                long long l = foldConst(e->lhs.get());
                long long r = foldConst(e->rhs.get());
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

    bool hasRuntimeCall(const Expr *e) const {
        if (!e) return false;
        if (e->kind == Expr::Kind::Call) {
            bool callFreeArgs = true;
            for (auto &arg : e->args) callFreeArgs = callFreeArgs && !hasCall(arg.get());
            if (!callFreeArgs ||
                (!inlineableFuncs.count(e->name) && !branchInlineableFuncs.count(e->name))) {
                return true;
            }
            return false;
        }
        if (hasRuntimeCall(e->lhs.get()) || hasRuntimeCall(e->rhs.get())) return true;
        for (auto &arg : e->args) {
            if (hasRuntimeCall(arg.get())) return true;
        }
        return false;
    }

    bool stmtContainsRuntimeCall(const Stmt *s) const {
        if (!s) return false;
        if (hasRuntimeCall(s->expr.get()) ||
            (s->decl && hasRuntimeCall(s->decl->init.get()))) {
            return true;
        }
        for (auto &child : s->stmts) {
            if (stmtContainsRuntimeCall(child.get())) return true;
        }
        return stmtContainsRuntimeCall(s->thenStmt.get()) ||
               stmtContainsRuntimeCall(s->elseStmt.get()) ||
               stmtContainsRuntimeCall(s->body.get());
    }

    int maxBranchInlineArgs(const Expr *e) const {
        if (!e) return 0;
        int result = 0;
        if (e->kind == Expr::Kind::Call && branchInlineableFuncs.count(e->name) &&
            !inlineableFuncs.count(e->name)) {
            bool pureArgs = true;
            for (auto &arg : e->args) pureArgs = pureArgs && !exprHasCall(arg.get());
            if (pureArgs) {
                for (auto &arg : e->args) {
                    if (arg->kind != Expr::Kind::Number && arg->kind != Expr::Kind::Var) {
                        ++result;
                    }
                }
            }
        }
        result = max(result, maxBranchInlineArgs(e->lhs.get()));
        result = max(result, maxBranchInlineArgs(e->rhs.get()));
        for (auto &arg : e->args) result = max(result, maxBranchInlineArgs(arg.get()));
        return result;
    }

    int maxBranchInlineArgs(const Stmt *s) const {
        if (!s) return 0;
        int result = maxBranchInlineArgs(s->expr.get());
        if (s->decl) result = max(result, maxBranchInlineArgs(s->decl->init.get()));
        for (auto &child : s->stmts) result = max(result, maxBranchInlineArgs(child.get()));
        result = max(result, maxBranchInlineArgs(s->thenStmt.get()));
        result = max(result, maxBranchInlineArgs(s->elseStmt.get()));
        result = max(result, maxBranchInlineArgs(s->body.get()));
        return result;
    }

    int maxLoopInlineScratch(const Expr *e) const {
        if (!e) return 0;
        int result = 0;
        if (e->kind == Expr::Kind::Call) {
            auto found = loopInlineableFuncs.find(e->name);
            if (found != loopInlineableFuncs.end() &&
                e->args.size() == found->second->params.size()) {
                bool eligibleArgs = true;
                for (auto &arg : e->args) {
                    eligibleArgs = eligibleArgs && !hasCall(arg.get());
                }
                if (eligibleArgs) result = found->second->fastLocalCount;
            }
        }
        result = max(result, maxLoopInlineScratch(e->lhs.get()));
        result = max(result, maxLoopInlineScratch(e->rhs.get()));
        for (auto &arg : e->args) {
            result = max(result, maxLoopInlineScratch(arg.get()));
        }
        return result;
    }

    int maxLoopInlineScratch(const Stmt *s) const {
        if (!s) return 0;
        int result = maxLoopInlineScratch(s->expr.get());
        if (s->decl) result = max(result, maxLoopInlineScratch(s->decl->init.get()));
        for (auto &child : s->stmts) {
            result = max(result, maxLoopInlineScratch(child.get()));
        }
        result = max(result, maxLoopInlineScratch(s->thenStmt.get()));
        result = max(result, maxLoopInlineScratch(s->elseStmt.get()));
        result = max(result, maxLoopInlineScratch(s->body.get()));
        return result;
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
        if (nextVarReg >= static_cast<int>(allocableVarRegs.size())) return "";
        return allocableVarRegs[nextVarReg++];
    }

    string allocVarRegForSlot(int slot) {
        if (!useSlotRegMap) return allocVarReg();
        auto found = slotRegMap.find(slot);
        return found == slotRegMap.end() ? string() : found->second;
    }

    string constReg(long long v) const {
        auto it = constRegMap.find(v);
        return it == constRegMap.end() ? string() : it->second;
    }

    void emitLoadConst(const string &dst, long long v) {
        string cr = constReg(v);
        if (!cr.empty()) {
            if (dst != cr) emit("mv " + dst + ", " + cr);
            return;
        }
        emit("li " + dst + ", " + to_string(v));
    }

    // ---- loop-invariant constant hoisting ----
    //
    // Constants materialized inside loops (multiplier/modulus immediates,
    // magic-division multipliers, loop bounds) cost an li (or lui+addi) every
    // iteration. Spare callee-saved registers not needed for variables are
    // loaded once in the function prologue and used as operands in place.

    void hoistNoteConst(long long v, long long weight, unordered_map<long long, long long> &w) const {
        if (v == 0) return;  // x0 already serves as zero
        w[v] += weight * (fits12(v) ? 1 : 2);
    }

    void hoistScanBranchInlineStmt(const Stmt *s, long long mult,
                                   unordered_map<long long, long long> &w) const {
        if (!s) return;
        if (s->expr) hoistScanExpr(s->expr.get(), mult, w);
        for (auto &child : s->stmts) hoistScanBranchInlineStmt(child.get(), mult, w);
        hoistScanBranchInlineStmt(s->thenStmt.get(), mult, w);
        hoistScanBranchInlineStmt(s->elseStmt.get(), mult, w);
    }

    void hoistScanLoopInlineStmt(const Stmt *s, int loopDepth,
                                 long long outerMult,
                                 unordered_map<long long, long long> &w) const {
        if (!s || s->fastDeadStore) return;
        long long mult = outerMult * (1LL << (3 * min(loopDepth, 7)));
        if (s->kind == Stmt::Kind::While) {
            long long inner = outerMult * (1LL << (3 * min(loopDepth + 1, 7)));
            hoistScanExpr(s->expr.get(), inner, w);
            hoistScanLoopInlineStmt(s->body.get(), loopDepth + 1, outerMult, w);
            return;
        }
        if (s->expr) hoistScanExpr(s->expr.get(), mult, w);
        if (s->decl && s->decl->init) hoistScanExpr(s->decl->init.get(), mult, w);
        for (auto &child : s->stmts) {
            hoistScanLoopInlineStmt(child.get(), loopDepth, outerMult, w);
        }
        hoistScanLoopInlineStmt(s->thenStmt.get(), loopDepth, outerMult, w);
        hoistScanLoopInlineStmt(s->elseStmt.get(), loopDepth, outerMult, w);
        hoistScanLoopInlineStmt(s->body.get(), loopDepth, outerMult, w);
    }

    void hoistScanExpr(const Expr *e, long long mult, unordered_map<long long, long long> &w) const {
        if (!e) return;
        if (e->kind == Expr::Kind::Call) {
            for (auto &arg : e->args) hoistScanExpr(arg.get(), mult, w);
            auto branch = branchInlineableFuncs.find(e->name);
            if (branch != branchInlineableFuncs.end() && !inlineableFuncs.count(e->name)) {
                bool pureArgs = true;
                for (auto &arg : e->args) pureArgs = pureArgs && !exprHasCall(arg.get());
                if (pureArgs) hoistScanBranchInlineStmt(branch->second->body.get(), mult, w);
            }
            auto loop = loopInlineableFuncs.find(e->name);
            if (loop != loopInlineableFuncs.end()) {
                bool pureArgs = true;
                for (auto &arg : e->args) pureArgs = pureArgs && !exprHasCall(arg.get());
                if (pureArgs) {
                    hoistScanLoopInlineStmt(loop->second->body.get(), 0,
                                            max(1LL, mult), w);
                }
            }
            return;
        }
        if (e->kind == Expr::Kind::Number) {
            hoistNoteConst(e->value, mult, w);
            return;
        }
        if (e->kind == Expr::Kind::Binary && e->rhs && e->rhs->kind == Expr::Kind::Number) {
            long long v = e->rhs->value;
            bool counted = false;
            if ((e->op == "+" || e->op == "-") && fits12(e->op == "+" ? v : -v)) counted = true;  // addi
            else if (e->op == "*" && (v == 0 || v == 1 || isPowerOfTwo(v))) counted = true;       // slli
            else if ((e->op == "/" || e->op == "%") && v > 0) {
                if (isPowerOfTwo(v)) counted = true;  // shift/mask sequence
                else if (auto spec = codegenMagicForDivisor(static_cast<int>(v))) {
                    hoistNoteConst(spec->magic, mult, w);
                    if (e->op == "%") hoistNoteConst(v, mult, w);
                    counted = true;
                }
            }
            // Relational constants are NOT skipped: branch codegen needs the
            // value in a register even when the slti immediate form would fit.
            if (!counted) hoistNoteConst(v, mult, w);
            hoistScanExpr(e->lhs.get(), mult, w);
            return;
        }
        hoistScanExpr(e->lhs.get(), mult, w);
        hoistScanExpr(e->rhs.get(), mult, w);
        for (auto &arg : e->args) hoistScanExpr(arg.get(), mult, w);
    }

    void hoistScanStmt(const Stmt *s, int depth, unordered_map<long long, long long> &w) const {
        if (!s || s->fastDeadStore) return;
        long long mult = depth > 0 ? (1LL << (3 * min(depth, 7))) : 0;
        if (s->kind == Stmt::Kind::While) {
            long long inner = 1LL << (3 * min(depth + 1, 7));
            hoistScanExpr(s->expr.get(), inner, w);
            hoistScanStmt(s->body.get(), depth + 1, w);
            return;
        }
        if (mult > 0) {
            if (s->expr) hoistScanExpr(s->expr.get(), mult, w);
            if (s->decl && s->decl->init) hoistScanExpr(s->decl->init.get(), mult, w);
        }
        for (auto &child : s->stmts) hoistScanStmt(child.get(), depth, w);
        hoistScanStmt(s->thenStmt.get(), depth, w);
        hoistScanStmt(s->elseStmt.get(), depth, w);
        hoistScanStmt(s->body.get(), depth, w);
    }

    vector<pair<long long, long long>> rankHoistConsts(const Function &f) const {
        unordered_map<long long, long long> w;
        hoistScanStmt(f.body.get(), 0, w);
        vector<pair<long long, long long>> ranked(w.begin(), w.end());
        sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) {
            if (a.second != b.second) return a.second > b.second;
            return a.first < b.first;
        });
        return ranked;
    }

    void enterScope() { scopes.push_back({}); }
    void leaveScope() { scopes.pop_back(); }

    static uint64_t slotUseWeight(int loopDepth) {
        return uint64_t{1} << (3 * min(loopDepth, 7));
    }

    static void addSlotUse(vector<uint64_t> &weights, int slot, uint64_t amount) {
        if (slot < 0 || slot >= static_cast<int>(weights.size())) return;
        uint64_t &value = weights[static_cast<size_t>(slot)];
        value = numeric_limits<uint64_t>::max() - value < amount
            ? numeric_limits<uint64_t>::max()
            : value + amount;
    }

    struct GlobalUse {
        uint64_t weight = 0;
        bool written = false;
    };

    void noteGlobalUse(unordered_map<string, GlobalUse> &uses, const string &name,
                       int loopDepth, bool written) const {
        auto found = globals.find(name);
        if (found == globals.end() || found->second.isConst) return;
        GlobalUse &use = uses[name];
        uint64_t amount = slotUseWeight(loopDepth);
        use.weight = numeric_limits<uint64_t>::max() - use.weight < amount
            ? numeric_limits<uint64_t>::max()
            : use.weight + amount;
        use.written = use.written || written;
    }

    void collectGlobalUses(const Expr *e, int loopDepth,
                           unordered_map<string, GlobalUse> &uses) const {
        if (!e) return;
        if (e->kind == Expr::Kind::Var && e->fastGlobal) {
            noteGlobalUse(uses, e->name, loopDepth, false);
        }
        collectGlobalUses(e->lhs.get(), loopDepth, uses);
        collectGlobalUses(e->rhs.get(), loopDepth, uses);
        for (auto &arg : e->args) collectGlobalUses(arg.get(), loopDepth, uses);
    }

    void collectGlobalUses(const Stmt *s, int loopDepth,
                           unordered_map<string, GlobalUse> &uses) const {
        if (!s) return;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) collectGlobalUses(child.get(), loopDepth, uses);
                return;
            case Stmt::Kind::Empty:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return;
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Return:
                if (!s->fastDeadStore) collectGlobalUses(s->expr.get(), loopDepth, uses);
                return;
            case Stmt::Kind::Assign:
                if (s->fastDeadStore) return;
                if (s->fastAssignGlobal) noteGlobalUse(uses, s->name, loopDepth, true);
                collectGlobalUses(s->expr.get(), loopDepth, uses);
                return;
            case Stmt::Kind::DeclStmt:
                if (!s->fastDeadStore && s->decl) {
                    collectGlobalUses(s->decl->init.get(), loopDepth, uses);
                }
                return;
            case Stmt::Kind::If:
                collectGlobalUses(s->expr.get(), loopDepth, uses);
                collectGlobalUses(s->thenStmt.get(), loopDepth, uses);
                collectGlobalUses(s->elseStmt.get(), loopDepth, uses);
                return;
            case Stmt::Kind::While:
                if (s->fastDeadStore) return;
                collectGlobalUses(s->expr.get(), loopDepth + 1, uses);
                collectGlobalUses(s->body.get(), loopDepth + 1, uses);
                return;
        }
    }

    vector<pair<string, GlobalUse>> rankGlobalCacheCandidates(const Function &f) const {
        unordered_map<string, GlobalUse> uses;
        collectGlobalUses(f.body.get(), 0, uses);
        vector<pair<string, GlobalUse>> ranked;
        for (auto &entry : uses) {
            if (entry.second.weight >= slotUseWeight(1)) ranked.push_back(entry);
        }
        sort(ranked.begin(), ranked.end(), [](const auto &lhs, const auto &rhs) {
            if (lhs.second.weight != rhs.second.weight) {
                return lhs.second.weight > rhs.second.weight;
            }
            return lhs.first < rhs.first;
        });
        return ranked;
    }

    void collectSlotUses(const Expr *e, int loopDepth, vector<uint64_t> &weights) const {
        if (!e) return;
        if (e->kind == Expr::Kind::Var && !e->fastGlobal) {
            addSlotUse(weights, e->fastIndex, slotUseWeight(loopDepth));
        }
        collectSlotUses(e->lhs.get(), loopDepth, weights);
        collectSlotUses(e->rhs.get(), loopDepth, weights);
        for (auto &arg : e->args) collectSlotUses(arg.get(), loopDepth, weights);
    }

    void collectSlotUses(const Stmt *s, int loopDepth, vector<uint64_t> &weights) const {
        if (!s) return;
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) collectSlotUses(child.get(), loopDepth, weights);
                return;
            case Stmt::Kind::Empty:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return;
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Return:
                if (!s->fastDeadStore) collectSlotUses(s->expr.get(), loopDepth, weights);
                return;
            case Stmt::Kind::Assign:
                if (s->fastDeadStore) return;
                if (!s->fastAssignGlobal) {
                    addSlotUse(weights, s->fastAssignIndex, slotUseWeight(loopDepth));
                }
                collectSlotUses(s->expr.get(), loopDepth, weights);
                return;
            case Stmt::Kind::DeclStmt:
                if (s->fastDeadStore) return;
                if (s->decl && !s->decl->isConst) {
                    addSlotUse(weights, s->decl->fastSlot, slotUseWeight(loopDepth));
                }
                if (s->decl) collectSlotUses(s->decl->init.get(), loopDepth, weights);
                return;
            case Stmt::Kind::If:
                collectSlotUses(s->expr.get(), loopDepth, weights);
                collectSlotUses(s->thenStmt.get(), loopDepth, weights);
                collectSlotUses(s->elseStmt.get(), loopDepth, weights);
                return;
            case Stmt::Kind::While:
                collectSlotUses(s->expr.get(), loopDepth + 1, weights);
                collectSlotUses(s->body.get(), loopDepth + 1, weights);
                return;
        }
    }

    void collectAllocatableSlots(const Stmt *s, int fastLocalCount,
                                 vector<int> &slots, bool &valid) const {
        if (!s || !valid) return;
        if (s->kind == Stmt::Kind::DeclStmt && s->decl && !s->decl->isConst) {
            int slot = s->decl->fastSlot;
            if (slot < 0 || slot >= fastLocalCount) {
                valid = false;
                return;
            }
            slots.push_back(slot);
        }
        for (auto &child : s->stmts) {
            collectAllocatableSlots(child.get(), fastLocalCount, slots, valid);
        }
        collectAllocatableSlots(s->thenStmt.get(), fastLocalCount, slots, valid);
        collectAllocatableSlots(s->elseStmt.get(), fastLocalCount, slots, valid);
        collectAllocatableSlots(s->body.get(), fastLocalCount, slots, valid);
    }

    void buildSlotRegisterMap(const Function &f, int slots,
                              const vector<string> &allVarRegs) {
        slotRegMap.clear();
        rankedSlotCandidates.clear();
        useSlotRegMap = false;
        if (f.fastLocalCount < static_cast<int>(f.params.size())) return;

        vector<int> candidates;
        for (int i = 0; i < static_cast<int>(f.params.size()); ++i) candidates.push_back(i);
        bool valid = true;
        collectAllocatableSlots(f.body.get(), f.fastLocalCount, candidates, valid);
        if (!valid) return;
        sort(candidates.begin(), candidates.end());
        candidates.erase(unique(candidates.begin(), candidates.end()), candidates.end());
        if (static_cast<int>(candidates.size()) != slots) return;

        vector<uint64_t> weights(static_cast<size_t>(f.fastLocalCount), 0);
        collectSlotUses(f.body.get(), 0, weights);
        sort(candidates.begin(), candidates.end(), [&](int lhs, int rhs) {
            if (weights[static_cast<size_t>(lhs)] != weights[static_cast<size_t>(rhs)]) {
                return weights[static_cast<size_t>(lhs)] > weights[static_cast<size_t>(rhs)];
            }
            return lhs < rhs;
        });
        for (int slot : candidates) {
            rankedSlotCandidates.push_back({slot, weights[static_cast<size_t>(slot)]});
        }
        for (int i = 0; i < varRegCap; ++i) {
            slotRegMap[candidates[static_cast<size_t>(i)]] = allVarRegs[static_cast<size_t>(i)];
        }
        useSlotRegMap = true;
    }

    void genFunction(Function &f) {
        cachedGlobals.clear();
        cachedGlobalRegs.clear();
        int slots = countSlots(f);
        loopInlineScratchSlots = maxLoopInlineScratch(f.body.get());
        static const vector<string> allSavedRegs = {
            "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11"
        };
        static const vector<string> leafRegs = {
            "a1", "a2", "a3", "a4", "a5", "a6", "a7"
        };
        bool callFree = !stmtContainsRuntimeCall(f.body.get());
        frameFreeLeaf = false;
        vector<string> allVarRegs;
        vector<pair<long long, long long>> rankedConsts = rankHoistConsts(f);
        rankedConsts.erase(
            remove_if(rankedConsts.begin(), rankedConsts.end(),
                      [](const auto &entry) { return entry.second < 8; }),
            rankedConsts.end());
        if (rankedConsts.size() > 8) rankedConsts.resize(8);

        // A small leaf can keep every mutable slot in caller-saved registers.
        // Probe the normal slot metadata first; any uncertainty falls back to
        // the existing framed ABI.
        if (callFree && slots <= static_cast<int>(leafRegs.size()) &&
            f.params.size() <= leafRegs.size()) {
            allVarRegs = leafRegs;
            varRegCap = slots;
            buildSlotRegisterMap(f, slots, allVarRegs);
            frameFreeLeaf = useSlotRegMap && varRegCap == slots;
        }
        // A prologue is paid once, while an unhoisted constant is paid on every
        // iteration. Prefer the wider framed register set when a hot leaf loop
        // cannot hold both all mutable slots and its loop constants in a1-a7.
        if (frameFreeLeaf && f.name == "main" &&
            static_cast<int>(rankedConsts.size()) >
                static_cast<int>(leafRegs.size()) - varRegCap) {
            frameFreeLeaf = false;
        }
        if (!frameFreeLeaf) {
            if (callFree) {
                allVarRegs = leafRegs;
                allVarRegs.insert(allVarRegs.end(), allSavedRegs.begin(), allSavedRegs.end());
            } else {
                allVarRegs = allSavedRegs;
            }
            varRegCap = min(static_cast<int>(allVarRegs.size()), slots);
            buildSlotRegisterMap(f, slots, allVarRegs);
        }
        int registerCount = static_cast<int>(allVarRegs.size());
        int desiredInlineArgs = maxBranchInlineArgs(f.body.get());
        int inlineArgCount = frameFreeLeaf
            ? 0
            : min(desiredInlineArgs, max(0, registerCount - slots));
        vector<long long> hoistVals;

        if (frameFreeLeaf) {
            int hoistCount = min({4, registerCount - varRegCap,
                                  static_cast<int>(rankedConsts.size())});
            for (int i = 0; i < hoistCount; ++i) hoistVals.push_back(rankedConsts[i].first);
        } else if (useSlotRegMap) {
            struct RegChoice {
                uint64_t score;
                bool isConst;
                int slot;
                long long value;
            };
            vector<RegChoice> choices;
            for (auto &[slot, score] : rankedSlotCandidates) {
                choices.push_back(RegChoice{score, false, slot, 0});
            }
            for (auto &[value, score] : rankedConsts) {
                choices.push_back(RegChoice{static_cast<uint64_t>(score), true, -1, value});
            }
            sort(choices.begin(), choices.end(), [](const RegChoice &lhs, const RegChoice &rhs) {
                if (lhs.score != rhs.score) return lhs.score > rhs.score;
                if (lhs.isConst != rhs.isConst) return !lhs.isConst;
                return lhs.isConst ? lhs.value < rhs.value : lhs.slot < rhs.slot;
            });
            int capacity = registerCount - inlineArgCount;
            if (static_cast<int>(choices.size()) > capacity) choices.resize(capacity);
            varRegCap = 0;
            for (const RegChoice &choice : choices) {
                if (choice.isConst) hoistVals.push_back(choice.value);
                else ++varRegCap;
            }
            buildSlotRegisterMap(f, slots, allVarRegs);
        } else {
            varRegCap = min(registerCount, slots);
            inlineArgCount = min(desiredInlineArgs, registerCount - varRegCap);
            int hoistCount = min({4, registerCount - varRegCap - inlineArgCount,
                                  static_cast<int>(rankedConsts.size())});
            for (int i = 0; i < hoistCount; ++i) hoistVals.push_back(rankedConsts[i].first);
        }
        inlineArgRegs.assign(allVarRegs.begin() + varRegCap,
                             allVarRegs.begin() + varRegCap + inlineArgCount);
        constRegMap.clear();
        hoistedConsts.clear();
        for (size_t i = 0; i < hoistVals.size(); ++i) {
            const string &reg = allVarRegs[varRegCap + inlineArgCount + static_cast<int>(i)];
            constRegMap[hoistVals[i]] = reg;
            hoistedConsts.push_back({hoistVals[i], reg});
        }
        int usedRegCount = varRegCap + inlineArgCount + static_cast<int>(hoistVals.size());
        if (callFree && usedRegCount < registerCount) {
            auto rankedGlobals = rankGlobalCacheCandidates(f);
            int cacheCount = min(registerCount - usedRegCount,
                                 static_cast<int>(rankedGlobals.size()));
            for (int i = 0; i < cacheCount; ++i) {
                auto global = globals.find(rankedGlobals[static_cast<size_t>(i)].first);
                if (global == globals.end() || global->second.isConst) continue;
                const string &reg = allVarRegs[static_cast<size_t>(usedRegCount++)];
                cachedGlobalRegs[global->first] = reg;
                cachedGlobals.push_back(CachedGlobal{
                    global->first, global->second.label, reg,
                    rankedGlobals[static_cast<size_t>(i)].second.written,
                });
            }
        }
        loopInlineHomeRegs.clear();
        if (!frameFreeLeaf && loopInlineScratchSlots > 0) {
            unordered_set<string> usedRegs;
            for (int i = 0; i < usedRegCount; ++i) {
                usedRegs.insert(allVarRegs[static_cast<size_t>(i)]);
            }
            for (const string &reg : allSavedRegs) {
                if (!usedRegs.count(reg)) loopInlineHomeRegs.push_back(reg);
            }
            for (const string &reg : leafRegs) {
                if (!usedRegs.count(reg)) loopInlineHomeRegs.push_back(reg);
            }
        }
        savedVarRegs.clear();
        if (!frameFreeLeaf) {
            for (int i = 0; i < usedRegCount; ++i) {
                const string &reg = allVarRegs[static_cast<size_t>(i)];
                if (!reg.empty() && reg[0] == 's') savedVarRegs.push_back(reg);
            }
            for (const string &reg : loopInlineHomeRegs) {
                if (!reg.empty() && reg[0] == 's' &&
                    find(savedVarRegs.begin(), savedVarRegs.end(), reg) ==
                        savedVarRegs.end()) {
                    savedVarRegs.push_back(reg);
                }
            }
        }
        allocableVarRegs.assign(allVarRegs.begin(), allVarRegs.begin() + varRegCap);
        frameSize = frameFreeLeaf
            ? 0
            : alignTo(8 + static_cast<int>(savedVarRegs.size()) * 4 +
                          (slots + loopInlineScratchSlots) * 4,
                      16);
        nextSlot = 0;
        nextVarReg = 0;
        localBaseOffset = -12 - static_cast<int>(savedVarRegs.size()) * 4;
        loopInlineScratchBaseOffset = localBaseOffset - slots * 4;
        scopes.clear();
        breakLabels.clear();
        continueLabels.clear();
        currentFunctionName = f.name;
        currentParams = f.params;
        returnLabel = newLabel("return_" + f.name);
        functionBodyLabel = newLabel("body_" + f.name);

        out << ".globl " << f.name << "\n";
        emitLabel(f.name);
        if (!frameFreeLeaf) {
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
        }

        enterScope();
        if (frameFreeLeaf) {
            vector<pair<string, string>> moves;
            moves.reserve(f.params.size());
            for (int i = 0; i < static_cast<int>(f.params.size()); ++i) {
                string reg = allocVarRegForSlot(i);
                if (reg.empty() || i >= 8) throw logic_error("invalid frame-free leaf parameter home");
                scopes.back()[f.params[i]] = Symbol{false, 0, false, "", 0, reg};
                moves.push_back({reg, "a" + to_string(i)});
            }
            emitParallelMoves(std::move(moves));
        } else if (callFree && varRegCap > 0) {
            vector<pair<string, string>> registerMoves;
            vector<pair<string, int>> stackParamHomes;
            for (int i = 0; i < static_cast<int>(f.params.size()); ++i) {
                string reg = allocVarRegForSlot(i);
                int off = reg.empty() ? allocSlot() : 0;
                scopes.back()[f.params[i]] = Symbol{false, 0, false, "", off, reg};
                if (i < 8) {
                    if (!reg.empty()) registerMoves.push_back({reg, "a" + to_string(i)});
                    else storeMem("a" + to_string(i), "s0", off);
                } else {
                    if (!reg.empty()) stackParamHomes.push_back({reg, (i - 8) * 4});
                    else {
                        loadMem("t0", "s0", (i - 8) * 4);
                        storeMem("t0", "s0", off);
                    }
                }
            }
            emitParallelMoves(std::move(registerMoves));
            for (auto &[reg, sourceOffset] : stackParamHomes) {
                loadMem(reg, "s0", sourceOffset);
            }
        } else {
            for (int i = 0; i < static_cast<int>(f.params.size()); ++i) {
                string reg = allocVarRegForSlot(i);
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
        }

        for (auto &[value, reg] : hoistedConsts) {
            emit("li " + reg + ", " + to_string(value));
        }
        for (const CachedGlobal &global : cachedGlobals) {
            emit("la t0, " + global.label);
            loadMem(global.reg, "t0", 0);
        }

        emitLabel(functionBodyLabel);
        genStmt(f.body.get());
        emit("li a0, 0");
        emitLabel(returnLabel);
        for (const CachedGlobal &global : cachedGlobals) {
            if (!global.written) continue;
            emit("la t0, " + global.label);
            storeMem(global.reg, "t0", 0);
        }
        if (!frameFreeLeaf) {
            for (int i = 0; i < static_cast<int>(savedVarRegs.size()); ++i) {
                loadMem(savedVarRegs[i], "s0", -12 - i * 4);
            }
            loadMem("ra", "s0", -4);
            loadMem("s0", "s0", -8);
            adjustSp(frameSize);
        }
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
                if (s->fastDeadStore || !hasCall(s->expr.get())) break;
                genExpr(s->expr.get());
                break;
            case Stmt::Kind::Assign: {
                if (s->fastDeadStore) break;
                const Expr *rhs = s->expr.get();
                // Call-free, short-circuit-free right-hand sides can target the
                // variable's home register directly: every read in such an
                // expression is emitted before the first write of the target,
                // so `x = f(x)` still reads the old value.
                if (!hasCall(rhs) && !exprHasShortCircuit(rhs)) {
                    auto sym = lookup(s->name);
                    if (sym && !sym->isConst && !sym->reg.empty()) {
                        genExprNoCall(rhs, sym->reg, {"t0", "t1", "t2", "t3", "t4", "t5"});
                        break;
                    }
                }
                genExpr(rhs);
                storeVar(s->name);
                break;
            }
            case Stmt::Kind::DeclStmt:
                genDecl(*s->decl, s->fastDeadStore);
                break;
            case Stmt::Kind::If:
                genIf(s);
                break;
            case Stmt::Kind::While:
                if (s->fastDeadStore) break;
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

    void genDecl(const Decl &d, bool skipInit = false) {
        if (d.isConst) {
            long long v = foldConst(d.init.get());
            scopes.back()[d.name] = Symbol{true, v, false, "", 0, ""};
            return;
        }
        string reg = allocVarRegForSlot(d.fastSlot);
        int off = reg.empty() ? allocSlot() : 0;
        scopes.back()[d.name] = Symbol{false, 0, false, "", off, reg};
        if (skipInit) return;
        const Expr *init = d.init.get();
        if (!reg.empty() && !hasCall(init) && !exprHasShortCircuit(init) &&
            !exprReadsVarName(init, d.name)) {
            genExprNoCall(init, reg, {"t0", "t1", "t2", "t3", "t4", "t5"});
            return;
        }
        genExpr(init);
        if (!reg.empty()) emit("mv " + reg + ", a0");
        else storeMem("a0", "s0", off);
    }

    static bool exprHasShortCircuit(const Expr *e) {
        if (!e) return false;
        if (e->kind == Expr::Kind::Binary && (e->op == "&&" || e->op == "||")) return true;
        if (exprHasShortCircuit(e->lhs.get()) || exprHasShortCircuit(e->rhs.get())) return true;
        for (auto &arg : e->args) {
            if (exprHasShortCircuit(arg.get())) return true;
        }
        return false;
    }

    static bool exprReadsVarName(const Expr *e, const string &name) {
        if (!e) return false;
        if (e->kind == Expr::Kind::Var && e->name == name) return true;
        if (exprReadsVarName(e->lhs.get(), name) || exprReadsVarName(e->rhs.get(), name)) return true;
        for (auto &arg : e->args) {
            if (exprReadsVarName(arg.get(), name)) return true;
        }
        return false;
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
        auto constCond = tryConst(s->expr.get());
        if (constCond && !*constCond) return;
        // Rotated loop: guard test up front, condition test at the bottom.
        // Saves the unconditional back-jump every iteration at the cost of
        // duplicating the condition once.
        string bodyLabel = newLabel("while_body");
        string condLabel = newLabel("while_cond");
        string endLabel = newLabel("while_end");
        if (!constCond) genCondFalse(s->expr.get(), endLabel);
        emitLabel(bodyLabel);
        breakLabels.push_back(endLabel);
        continueLabels.push_back(condLabel);
        genStmt(s->body.get());
        continueLabels.pop_back();
        breakLabels.pop_back();
        emitLabel(condLabel);
        if (constCond) emit("j " + bodyLabel);
        else genCondTrue(s->expr.get(), bodyLabel);
        emitLabel(endLabel);
    }

    void genExpr(const Expr *e) {
        if (auto v = tryConst(e)) {
            emitLoadConst("a0", *v);
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
            emitLoadConst(dst, sym->constValue);
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

    struct CodegenMagicDiv {
        int divisor;
        int32_t magic;
        int shift;
        bool addDividend;
        bool logicalShift;
    };

    static optional<CodegenMagicDiv> codegenMagicForDivisor(int divisor) {
        if (divisor <= 1) return nullopt;
        const int64_t two31 = 1ll << 31;
        const int64_t ad = divisor;
        const int64_t anc = two31 - 1 - (two31 - 1) % ad;
        int p = 31;
        int64_t q1 = two31 / anc;
        int64_t r1 = two31 - q1 * anc;
        int64_t q2 = two31 / ad;
        int64_t r2 = two31 - q2 * ad;
        int64_t delta = 0;
        do {
            ++p;
            q1 <<= 1;
            r1 <<= 1;
            if (r1 >= anc) {
                ++q1;
                r1 -= anc;
            }
            q2 <<= 1;
            r2 <<= 1;
            if (r2 >= ad) {
                ++q2;
                r2 -= ad;
            }
            delta = ad - r2;
        } while (q1 < delta || (q1 == delta && r1 == 0));

        int32_t magic = static_cast<int32_t>(static_cast<uint32_t>(q2 + 1));
        int shift = p - 32;
        bool addDividend = magic < 0;
        return CodegenMagicDiv{
            divisor,
            magic,
            addDividend ? shift : shift + 32,
            addDividend,
            addDividend,
        };
    }

    bool canStrengthReduceDivisor(long long divisor) const {
        if (divisor <= 0 || divisor > INT32_MAX) return false;
        if (isPowerOfTwo(divisor) && divisor <= 2048) return true;
        return codegenMagicForDivisor(static_cast<int>(divisor)).has_value();
    }

    static bool regBlocked(const string &reg, initializer_list<string> blocked) {
        for (const string &b : blocked) {
            if (reg == b) return true;
        }
        return false;
    }

    string pickScratch(initializer_list<string> blocked, const vector<string> &preferred = {}) const {
        for (const string &r : preferred) {
            if (!regBlocked(r, blocked)) return r;
        }
        static const vector<string> regs = {"t0", "t1", "t2", "t3", "t4", "t5", "t6"};
        for (const string &r : regs) {
            if (!regBlocked(r, blocked)) return r;
        }
        return "t6";
    }

    void emitQuotientConst(const string &dst, const string &src, int divisor, const string &tmp) {
        if (isPowerOfTwo(divisor)) {
            if (divisor == 1) {
                if (dst != src) emit("mv " + dst + ", " + src);
                return;
            }
            int shift = log2Int(divisor);
            emit("srai " + tmp + ", " + src + ", 31");
            if (fits12(divisor - 1)) {
                emit("andi " + tmp + ", " + tmp + ", " + to_string(divisor - 1));
            } else {
                // The sign word is either zero or all ones. A logical shift
                // materializes the large (2^shift - 1) bias without an
                // out-of-range I-type immediate.
                emit("srli " + tmp + ", " + tmp + ", " + to_string(32 - shift));
            }
            emit("add " + dst + ", " + src + ", " + tmp);
            emit("srai " + dst + ", " + dst + ", " + to_string(shift));
            return;
        }
        auto spec = codegenMagicForDivisor(divisor);
        if (!spec) {
            string dreg = constReg(divisor);
            if (dreg.empty()) {
                emit("li " + tmp + ", " + to_string(divisor));
                dreg = tmp;
            }
            emit("div " + dst + ", " + src + ", " + dreg);
            return;
        }
        string mreg = constReg(spec->magic);
        if (mreg.empty()) {
            emit("li " + tmp + ", " + to_string(spec->magic));
            mreg = tmp;
        }
        emit("mulh " + dst + ", " + src + ", " + mreg);
        int postShift = spec->logicalShift
            ? (spec->addDividend ? spec->shift : spec->shift - 32)
            : spec->shift - 32;
        if (spec->addDividend) emit("add " + dst + ", " + dst + ", " + src);
        if (postShift > 0) emit("srai " + dst + ", " + dst + ", " + to_string(postShift));
        emit("srai " + tmp + ", " + src + ", 31");
        emit("sub " + dst + ", " + dst + ", " + tmp);
    }

    void emitDivModConst(const string &dst, const string &src, int divisor, bool isMod,
                         bool sourceNonNegative,
                         const vector<string> &scratchPrefs = {}) {
        if (sourceNonNegative && isPowerOfTwo(divisor)) {
            int shift = log2Int(divisor);
            if (!isMod) {
                emit("srli " + dst + ", " + src + ", " + to_string(shift));
            } else if (fits12(divisor - 1)) {
                emit("andi " + dst + ", " + src + ", " + to_string(divisor - 1));
            } else {
                emit("slli " + dst + ", " + src + ", " + to_string(32 - shift));
                emit("srli " + dst + ", " + dst + ", " + to_string(32 - shift));
            }
            return;
        }
        if (!isMod) {
            if (dst == src) {
                string orig = pickScratch({dst, src}, scratchPrefs);
                string tmp = pickScratch({dst, src, orig}, scratchPrefs);
                emit("mv " + orig + ", " + src);
                emitQuotientConst(dst, orig, divisor, tmp);
            } else {
                string tmp = pickScratch({dst, src}, scratchPrefs);
                emitQuotientConst(dst, src, divisor, tmp);
            }
            return;
        }
        if (divisor == 1) {
            emit("li " + dst + ", 0");
            return;
        }
        if (dst != src) {
            string tmp = pickScratch({dst, src}, scratchPrefs);
            emitQuotientConst(dst, src, divisor, tmp);
            if (isPowerOfTwo(divisor)) {
                emit("slli " + dst + ", " + dst + ", " + to_string(log2Int(divisor)));
            } else {
                string dreg = constReg(divisor);
                if (dreg.empty()) {
                    emit("li " + tmp + ", " + to_string(divisor));
                    dreg = tmp;
                }
                emit("mul " + dst + ", " + dst + ", " + dreg);
            }
            emit("sub " + dst + ", " + src + ", " + dst);
            return;
        }
        string orig = pickScratch({dst, src}, scratchPrefs);
        string tmp = pickScratch({dst, src, orig}, scratchPrefs);
        emit("mv " + orig + ", " + src);
        emitQuotientConst(dst, orig, divisor, tmp);
        if (isPowerOfTwo(divisor)) emit("slli " + dst + ", " + dst + ", " + to_string(log2Int(divisor)));
        else {
            string dreg = constReg(divisor);
            if (dreg.empty()) {
                emit("li " + tmp + ", " + to_string(divisor));
                dreg = tmp;
            }
            emit("mul " + dst + ", " + dst + ", " + dreg);
        }
        emit("sub " + dst + ", " + orig + ", " + dst);
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
            emitLoadConst(dst, *v);
            return;
        }
        switch (e->kind) {
            case Expr::Kind::Number:
                emitLoadConst(dst, e->value);
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

        // Short-circuit operators are evaluated entirely within dst + the
        // provided register pool: genCondTrue/genCondFalse hardcode a0/t0 and
        // would clobber live values held by an enclosing genExprNoCall.
        if (e->op == "&&") {
            string falseLabel = newLabel("land_false");
            string endLabel = newLabel("land_end");
            genExprNoCall(e->lhs.get(), dst, regs);
            emit("beqz " + dst + ", " + falseLabel);
            genExprNoCall(e->rhs.get(), dst, regs);
            emit("sltu " + dst + ", x0, " + dst);
            emit("j " + endLabel);
            emitLabel(falseLabel);
            emit("li " + dst + ", 0");
            emitLabel(endLabel);
            return;
        }
        if (e->op == "||") {
            string trueLabel = newLabel("lor_true");
            string endLabel = newLabel("lor_end");
            genExprNoCall(e->lhs.get(), dst, regs);
            emit("bnez " + dst + ", " + trueLabel);
            genExprNoCall(e->rhs.get(), dst, regs);
            emit("sltu " + dst + ", x0, " + dst);
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
            if ((e->op == "/" || e->op == "%") && canStrengthReduceDivisor(*rhs)) {
                string sourceReg;
                if (e->lhs->kind == Expr::Kind::Var) {
                    auto symbol = lookup(e->lhs->name);
                    if (symbol && !symbol->isConst && !symbol->reg.empty() &&
                        symbol->reg != dst) {
                        sourceReg = symbol->reg;
                    }
                }
                if (sourceReg.empty()) {
                    genExprNoCall(e->lhs.get(), dst, regs);
                    sourceReg = dst;
                }
                bool nonNegative = e->lhs->rangeAnalyzed && e->lhs->rangeMin >= 0;
                if (nonNegative && e->lhs->rangeMax < *rhs) {
                    if (e->op == "/") emit("li " + dst + ", 0");
                    else if (sourceReg != dst) emit("mv " + dst + ", " + sourceReg);
                    return;
                }
                emitDivModConst(dst, sourceReg, static_cast<int>(*rhs), e->op == "%",
                                nonNegative, regs);
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

        // Operands already available in registers are used in place with no
        // evaluation code: zero (x0), hoisted constants, and variables living
        // in callee-saved registers. A home register must differ from dst,
        // since evaluating the opposite side may write dst first.
        auto inPlaceReg = [&](const Expr *operand) -> string {
            if (auto v = tryConst(operand)) {
                if (*v == 0) return "x0";
                return constReg(*v);
            }
            if (operand->kind == Expr::Kind::Var) {
                auto sym = lookup(operand->name);
                if (sym && !sym->isConst && !sym->reg.empty() && sym->reg != dst) {
                    return sym->reg;
                }
            }
            return string();
        };
        string lhsInPlace = inPlaceReg(e->lhs.get());
        string rhsInPlace = inPlaceReg(e->rhs.get());
        if (!lhsInPlace.empty() && !rhsInPlace.empty()) {
            emitBinaryReg(e->op, dst, lhsInPlace, rhsInPlace);
            return;
        }
        if (!lhsInPlace.empty()) {
            genExprNoCall(e->rhs.get(), dst, regs);
            emitBinaryReg(e->op, dst, lhsInPlace, dst);
            return;
        }
        if (!rhsInPlace.empty()) {
            genExprNoCall(e->lhs.get(), dst, regs);
            emitBinaryReg(e->op, dst, dst, rhsInPlace);
            return;
        }
        // The destination variable itself as an operand (x = x op e): keep it
        // in place, evaluate the other side into a temporary, combine last.
        auto isDstVar = [&](const Expr *operand) {
            if (operand->kind != Expr::Kind::Var) return false;
            auto sym = lookup(operand->name);
            return sym && !sym->isConst && sym->reg == dst;
        };
        if (!regs.empty() && isDstVar(e->lhs.get())) {
            string tmp = regs.front();
            vector<string> rest(regs.begin() + 1, regs.end());
            genExprNoCall(e->rhs.get(), tmp, rest);
            emitBinaryReg(e->op, dst, dst, tmp);
            return;
        }
        if (!regs.empty() && isDstVar(e->rhs.get())) {
            string tmp = regs.front();
            vector<string> rest(regs.begin() + 1, regs.end());
            genExprNoCall(e->lhs.get(), tmp, rest);
            emitBinaryReg(e->op, dst, tmp, dst);
            return;
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

    // Materialize a branch operand: constants of zero become x0, variables
    // already living in a callee-saved register are used in place, and
    // everything else is evaluated into the given scratch register.
    string condOperandReg(const Expr *e, const string &scratch, const vector<string> &pool) {
        if (auto v = tryConst(e)) {
            if (*v == 0) return "x0";
            string cr = constReg(*v);
            if (!cr.empty()) return cr;
            emit("li " + scratch + ", " + to_string(*v));
            return scratch;
        }
        if (e->kind == Expr::Kind::Var) {
            auto sym = lookup(e->name);
            if (sym && !sym->isConst && !sym->reg.empty()) return sym->reg;
        }
        genExprNoCall(e, scratch, pool);
        return scratch;
    }

    bool genPow2ModZeroBranch(const Expr *e, const string &label, bool jumpIfTrue) {
        if (!e || e->kind != Expr::Kind::Binary ||
            (e->op != "==" && e->op != "!=")) {
            return false;
        }
        const Expr *mod = nullptr;
        if (auto rhs = tryConst(e->rhs.get()); rhs && *rhs == 0) mod = e->lhs.get();
        else if (auto lhs = tryConst(e->lhs.get()); lhs && *lhs == 0) mod = e->rhs.get();
        if (!mod || mod->kind != Expr::Kind::Binary || mod->op != "%" ||
            hasCall(mod->lhs.get())) {
            return false;
        }
        auto divisor = tryConst(mod->rhs.get());
        if (!divisor || !isPowerOfTwo(*divisor) || *divisor > INT32_MAX) return false;

        string value = condOperandReg(mod->lhs.get(), "a0", {"t1", "t2", "t3", "t4", "t5"});
        int shift = log2Int(*divisor);
        if (fits12(*divisor - 1)) {
            emit("andi a0, " + value + ", " + to_string(*divisor - 1));
        } else {
            emit("slli a0, " + value + ", " + to_string(32 - shift));
        }
        bool expressionTrueWhenZero = e->op == "==";
        bool jumpOnZero = jumpIfTrue == expressionTrueWhenZero;
        emit(string(jumpOnZero ? "beqz a0, " : "bnez a0, ") + label);
        return true;
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
        if (genPow2ModZeroBranch(e, falseLabel, false)) return;
        static const unordered_set<string> relops = {"<", ">", "<=", ">=", "==", "!="};
        if (e->kind == Expr::Kind::Binary && relops.count(e->op) && !hasCall(e)) {
            string l = condOperandReg(e->lhs.get(), "a0", {"t1", "t2", "t3", "t4", "t5"});
            string r = condOperandReg(e->rhs.get(), "t0", {"t1", "t2", "t3", "t4", "t5"});
            if (e->op == "<") emit("bge " + l + ", " + r + ", " + falseLabel);
            else if (e->op == ">") emit("bge " + r + ", " + l + ", " + falseLabel);
            else if (e->op == "<=") emit("blt " + r + ", " + l + ", " + falseLabel);
            else if (e->op == ">=") emit("blt " + l + ", " + r + ", " + falseLabel);
            else if (e->op == "==") emit("bne " + l + ", " + r + ", " + falseLabel);
            else if (e->op == "!=") emit("beq " + l + ", " + r + ", " + falseLabel);
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
        if (genPow2ModZeroBranch(e, trueLabel, true)) return;
        static const unordered_set<string> relops = {"<", ">", "<=", ">=", "==", "!="};
        if (e->kind == Expr::Kind::Binary && relops.count(e->op) && !hasCall(e)) {
            string l = condOperandReg(e->lhs.get(), "a0", {"t1", "t2", "t3", "t4", "t5"});
            string r = condOperandReg(e->rhs.get(), "t0", {"t1", "t2", "t3", "t4", "t5"});
            if (e->op == "<") emit("blt " + l + ", " + r + ", " + trueLabel);
            else if (e->op == ">") emit("blt " + r + ", " + l + ", " + trueLabel);
            else if (e->op == "<=") emit("bge " + r + ", " + l + ", " + trueLabel);
            else if (e->op == ">=") emit("bge " + l + ", " + r + ", " + trueLabel);
            else if (e->op == "==") emit("beq " + l + ", " + r + ", " + trueLabel);
            else if (e->op == "!=") emit("bne " + l + ", " + r + ", " + trueLabel);
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
        bool argsHaveDivMod = false;
        for (auto &arg : e->args) {
            if (hasCall(arg.get())) {
                argsHaveCall = true;
            }
            if (exprHasDivMod(arg.get())) argsHaveDivMod = true;
        }
        static const vector<string> tailTemps = {"t0", "t1", "t2", "t3", "t4", "t5"};
        // Nested division/modulo strength reduction may escape the provided scratch pool.
        if (!argsHaveCall && !argsHaveDivMod && n + 2 <= static_cast<int>(tailTemps.size())) {
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
                if ((e->op == "/" || e->op == "%") && canStrengthReduceDivisor(*rhs)) {
                    genExpr(e->lhs.get());
                    bool nonNegative = e->lhs->rangeAnalyzed && e->lhs->rangeMin >= 0;
                    emitDivModConst("a0", "a0", static_cast<int>(*rhs), e->op == "%",
                                    nonNegative);
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

        // When one side is call-free (pure), evaluate the call side first and
        // the pure side afterwards into a temporary, avoiding the stack
        // round-trip. Function calls cannot modify caller locals, so a pure
        // side that reads no globals sees the same values either way; a
        // global-reading lhs must NOT be reordered after an rhs call that
        // could write those globals.
        string lhsReg = "t0";
        string rhsReg = "a0";
        if (!hasCall(e->lhs.get()) && !exprReadsAnyGlobal(e->lhs.get())) {
            genExpr(e->rhs.get());
            lhsReg = condOperandReg(e->lhs.get(), "t0", {"t1", "t2", "t3", "t4", "t5"});
        } else if (!hasCall(e->rhs.get())) {
            genExpr(e->lhs.get());
            lhsReg = "a0";
            rhsReg = condOperandReg(e->rhs.get(), "t0", {"t1", "t2", "t3", "t4", "t5"});
        } else {
            genExpr(e->lhs.get());
            pushA0();
            genExpr(e->rhs.get());
            popTo("t0");
        }
        emitBinaryReg(e->op, "a0", lhsReg, rhsReg);
    }

    bool exprReadsAnyGlobal(const Expr *e) const {
        if (!e) return false;
        if (e->kind == Expr::Kind::Var) {
            auto sym = lookup(e->name);
            return sym && sym->isGlobal;
        }
        if (exprReadsAnyGlobal(e->lhs.get()) || exprReadsAnyGlobal(e->rhs.get())) return true;
        for (auto &arg : e->args) {
            if (exprReadsAnyGlobal(arg.get())) return true;
        }
        return false;
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

    void genInlineBranchStmt(const Stmt *s,
                             const unordered_map<string, const Expr *> &subst,
                             const string &dst, const string &returnLabel) {
        switch (s->kind) {
            case Stmt::Kind::Block:
                for (auto &child : s->stmts) {
                    genInlineBranchStmt(child.get(), subst, dst, returnLabel);
                    if (stmtAlwaysJumps(child.get())) break;
                }
                return;
            case Stmt::Kind::Empty:
                return;
            case Stmt::Kind::If: {
                auto condition = cloneExprSubstGeneric(s->expr.get(), subst);
                string elseLabel = newLabel("inline_else");
                genCondFalse(condition.get(), elseLabel);
                genInlineBranchStmt(s->thenStmt.get(), subst, dst, returnLabel);
                if (s->elseStmt) {
                    string endLabel = newLabel("inline_endif");
                    emit("j " + endLabel);
                    emitLabel(elseLabel);
                    genInlineBranchStmt(s->elseStmt.get(), subst, dst, returnLabel);
                    emitLabel(endLabel);
                } else {
                    emitLabel(elseLabel);
                }
                return;
            }
            case Stmt::Kind::Return: {
                auto value = cloneExprSubstGeneric(s->expr.get(), subst);
                genExprNoCall(value.get(), dst, {"t0", "t1", "t2", "t3", "t4", "t5"});
                emit("j " + returnLabel);
                return;
            }
            case Stmt::Kind::ExprStmt:
            case Stmt::Kind::Assign:
            case Stmt::Kind::DeclStmt:
            case Stmt::Kind::While:
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                return;
        }
    }

    unordered_map<int, Symbol> loopInlineSlotHomes(const Function &function) const {
        unordered_map<int, Symbol> homes;
        auto ranked = loopInlineRegisterSlots.find(function.name);
        if (ranked == loopInlineRegisterSlots.end()) return homes;
        for (int slot = 0; slot < function.fastLocalCount; ++slot) {
            homes[slot] = Symbol{false, 0, false, "",
                                 loopInlineScratchBaseOffset - slot * 4, ""};
        }
        for (size_t rank = 0; rank < ranked->second.size(); ++rank) {
            if (rank >= loopInlineHomeRegs.size()) break;
            int slot = ranked->second[rank];
            auto found = homes.find(slot);
            if (found != homes.end()) found->second.reg = loopInlineHomeRegs[rank];
        }
        return homes;
    }

    void genInlineLoopStmt(const Stmt *s, const string &endLabel,
                           const unordered_map<int, Symbol> &slotHomes) {
        if (!s) return;
        switch (s->kind) {
            case Stmt::Kind::Block:
                enterScope();
                for (auto &child : s->stmts) {
                    genInlineLoopStmt(child.get(), endLabel, slotHomes);
                    if (stmtAlwaysJumps(child.get())) break;
                }
                leaveScope();
                return;
            case Stmt::Kind::Empty:
            case Stmt::Kind::ExprStmt:
                return;
            case Stmt::Kind::DeclStmt: {
                const Decl &decl = *s->decl;
                if (decl.isConst) {
                    long long value = foldConst(decl.init.get());
                    scopes.back()[decl.name] = Symbol{true, value, false, "", 0, ""};
                    return;
                }
                auto home = slotHomes.find(decl.fastSlot);
                if (home == slotHomes.end() || home->second.isConst ||
                    home->second.isGlobal) {
                    throw logic_error("invalid loop-inline local slot");
                }
                scopes.back()[decl.name] = home->second;
                genExprNoCall(decl.init.get(), "a0",
                              {"t0", "t1", "t2", "t3", "t4", "t5"});
                storeVarFrom(decl.name, "a0");
                return;
            }
            case Stmt::Kind::Assign: {
                auto symbol = lookup(s->name);
                if (!symbol || symbol->isConst) {
                    throw logic_error("invalid loop-inline assignment");
                }
                genExprNoCall(s->expr.get(), "a0",
                              {"t0", "t1", "t2", "t3", "t4", "t5"});
                storeVarFrom(s->name, "a0");
                return;
            }
            case Stmt::Kind::If: {
                string elseLabel = newLabel("loop_inline_else");
                string afterLabel = newLabel("loop_inline_endif");
                genCondFalse(s->expr.get(), elseLabel);
                genInlineLoopStmt(s->thenStmt.get(), endLabel, slotHomes);
                if (s->elseStmt) {
                    emit("j " + afterLabel);
                    emitLabel(elseLabel);
                    genInlineLoopStmt(s->elseStmt.get(), endLabel, slotHomes);
                    emitLabel(afterLabel);
                } else {
                    emitLabel(elseLabel);
                }
                return;
            }
            case Stmt::Kind::While: {
                auto constCond = tryConst(s->expr.get());
                if (constCond && !*constCond) return;
                string bodyLabel = newLabel("loop_inline_while_body");
                string condLabel = newLabel("loop_inline_while_cond");
                string afterLabel = newLabel("loop_inline_while_end");
                if (!constCond) genCondFalse(s->expr.get(), afterLabel);
                emitLabel(bodyLabel);
                breakLabels.push_back(afterLabel);
                continueLabels.push_back(condLabel);
                genInlineLoopStmt(s->body.get(), endLabel, slotHomes);
                continueLabels.pop_back();
                breakLabels.pop_back();
                emitLabel(condLabel);
                if (constCond) emit("j " + bodyLabel);
                else genCondTrue(s->expr.get(), bodyLabel);
                emitLabel(afterLabel);
                return;
            }
            case Stmt::Kind::Break:
                if (breakLabels.empty()) throw logic_error("invalid loop-inline break");
                emit("j " + breakLabels.back());
                return;
            case Stmt::Kind::Continue:
                if (continueLabels.empty()) throw logic_error("invalid loop-inline continue");
                emit("j " + continueLabels.back());
                return;
            case Stmt::Kind::Return:
                genExprNoCall(s->expr.get(), "a0",
                              {"t0", "t1", "t2", "t3", "t4", "t5"});
                emit("j " + endLabel);
                return;
        }
        throw logic_error("invalid loop-inline statement");
    }

    bool tryGenLoopInlineCall(const Expr *e, const string &dst) {
        if (dst != "a0") return false;
        auto found = loopInlineableFuncs.find(e->name);
        if (found == loopInlineableFuncs.end()) return false;
        Function *function = found->second;
        if (e->args.size() != function->params.size()) return false;
        for (auto &arg : e->args) {
            if (hasCall(arg.get())) return false;
        }

        unordered_map<int, Symbol> slotHomes = loopInlineSlotHomes(*function);
        if (static_cast<int>(slotHomes.size()) != function->fastLocalCount) {
            return false;
        }
        for (size_t i = 0; i < e->args.size(); ++i) {
            const Symbol &home = slotHomes[static_cast<int>(i)];
            if (!home.reg.empty()) {
                genExprNoCall(e->args[i].get(), home.reg,
                              {"t0", "t1", "t2", "t3", "t4", "t5"});
            } else {
                genExprNoCall(e->args[i].get(), "a0",
                              {"t0", "t1", "t2", "t3", "t4", "t5"});
                storeMem("a0", "s0", home.offset);
            }
        }

        auto callerScopes = std::move(scopes);
        scopes.clear();
        enterScope();
        for (size_t i = 0; i < function->params.size(); ++i) {
            scopes.back()[function->params[i]] = slotHomes[static_cast<int>(i)];
        }
        string endLabel = newLabel("loop_inline_return");
        genInlineLoopStmt(function->body.get(), endLabel, slotHomes);
        emitLabel(endLabel);
        leaveScope();
        scopes = std::move(callerScopes);
        return true;
    }

    bool tryGenInlineCall(const Expr *e, const string &dst) {
        if (!e || e->kind != Expr::Kind::Call) return false;
        auto found = inlineableFuncs.find(e->name);
        if (found != inlineableFuncs.end()) {
            Function *f = found->second;
            if (e->args.size() != f->params.size()) return false;
            for (auto &arg : e->args) {
                if (hasCall(arg.get())) return false;
            }
            unordered_map<string, const Expr *> subst;
            for (size_t i = 0; i < f->params.size(); ++i) {
                subst[f->params[i]] = e->args[i].get();
            }
            const Expr *ret = f->body->stmts[0]->expr.get();
            auto expanded = cloneExprSubstGeneric(ret, subst);
            genExprNoCall(expanded.get(), dst, {"t0", "t1", "t2", "t3", "t4", "t5"});
            return true;
        }

        // Branch inlining uses the normal condition generator, whose scratch
        // convention owns a0/t0. Calls nested in a larger expression already
        // route through a0, then move the completed result to their requested
        // destination.
        if (dst != "a0") return false;
        auto branch = branchInlineableFuncs.find(e->name);
        if (branch == branchInlineableFuncs.end()) return tryGenLoopInlineCall(e, dst);
        Function *f = branch->second;
        if (e->args.size() != f->params.size()) return false;
        for (auto &arg : e->args) {
            if (hasCall(arg.get())) return false;
        }
        unordered_map<string, const Expr *> subst;
        size_t neededRegs = 0;
        for (auto &arg : e->args) {
            if (tryConst(arg.get())) continue;
            if (arg->kind == Expr::Kind::Var && lookup(arg->name)) continue;
            ++neededRegs;
        }
        bool materializeArgs = neededRegs <= inlineArgRegs.size();
        if (materializeArgs) {
            vector<Symbol> paramSymbols;
            paramSymbols.reserve(e->args.size());
            size_t nextInlineReg = 0;
            for (auto &arg : e->args) {
                if (auto value = tryConst(arg.get())) {
                    paramSymbols.push_back(Symbol{true, *value, false, "", 0, ""});
                    continue;
                }
                if (arg->kind == Expr::Kind::Var) {
                    auto symbol = lookup(arg->name);
                    if (symbol) {
                        paramSymbols.push_back(*symbol);
                        continue;
                    }
                }
                const string &reg = inlineArgRegs[nextInlineReg++];
                genExprNoCall(arg.get(), reg, {"t0", "t1", "t2", "t3", "t4", "t5"});
                paramSymbols.push_back(Symbol{false, 0, false, "", 0, reg});
            }
            enterScope();
            for (size_t i = 0; i < f->params.size(); ++i) {
                scopes.back()[f->params[i]] = paramSymbols[i];
            }
        } else {
            for (size_t i = 0; i < f->params.size(); ++i) {
                subst[f->params[i]] = e->args[i].get();
            }
        }
        string endLabel = newLabel("inline_return");
        genInlineBranchStmt(f->body.get(), subst, dst, endLabel);
        emitLabel(endLabel);
        if (materializeArgs) leaveScope();
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
    (void)argc;
    (void)argv;

    string input((istreambuf_iterator<char>(cin)), istreambuf_iterator<char>());
    Lexer lexer(input);
    auto tokens = lexer.lex();
    Parser parser(std::move(tokens));
    Program program = parser.parseProgram();

    // 1. Rewrite the AST using only semantics-preserving local proofs.
    SafeOptimizer optimizer(program);
    optimizer.run();

    // 2. Resolve bindings/slots and run conservative static data-flow analysis.
    StaticAnalyzer analysis(program);
    analysis.run();

    // 3. Replace statically proven affine counted loops with closed-form
    // runtime assignments.  This transforms individual loops; it never calls
    // or executes a ToyC function.
    // Re-resolve after each rewrite round so an outer loop can consume the
    // closed-form state transition produced for a nested inner loop.
    for (int round = 0; round < 4; ++round) {
        AffineLoopOptimizer loopOptimizer(program);
        if (!loopOptimizer.run()) break;
        StaticAnalyzer rewrittenAnalysis(program);
        rewrittenAnalysis.run();
    }

    // 4. Prove ranges used by safe instruction-selection shortcuts.
    RangeAnalyzer rangeAnalysis(program);
    rangeAnalysis.run();

    // 5. Emit the actual runtime RISC-V32 program.
    CodeGen codegen(program);
    cout << codegen.generate();
    return 0;
}
