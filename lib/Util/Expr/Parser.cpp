// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Util/Expr/Parser.hpp"
#include "Util/Expr/Lexer.hpp"
#include <algorithm>
#include <array>
#include <initializer_list>

using namespace Pothos::Util::Expr;

namespace {

using NodePtr = std::unique_ptr<Node>;

bool isKeyword(const std::string_view name)
{
    static const std::array<std::string_view, 5> keywords{"and", "or", "not", "if", "else"};
    return std::ranges::find(keywords, name) != keywords.end();
}

class Parser
{
public:
    Parser(const std::string_view expr):
        _toks(tokenize(expr))
    {
        return;
    }

    NodePtr run(void)
    {
        if (cur().kind == Tok::End) fail("expression is empty", cur().pos);
        auto node = this->expr();
        if (cur().kind != Tok::End) this->unexpected();
        return node;
    }

private:
    //! Counts recursion into nested sub-expressions, fails past MaxNesting
    struct Nest
    {
        Nest(Parser &p):
            p(p)
        {
            if (++p._nesting > MaxNesting) p.fail("expression is nested too deeply", p.cur().pos);
        }
        ~Nest(void)
        {
            --p._nesting;
        }
        Parser &p;
    };

    const Token &cur(void) const
    {
        return _toks[_i];
    }

    bool isOp(const std::string_view op) const
    {
        return cur().kind == Tok::Op and cur().text == op;
    }

    bool isKw(const std::string_view kw) const
    {
        return cur().kind == Tok::Name and cur().text == kw;
    }

    bool isAnyOp(const std::initializer_list<std::string_view> ops) const
    {
        return cur().kind == Tok::Op and std::ranges::find(ops, cur().text) != ops.end();
    }

    [[noreturn]] void fail(const std::string &message, const std::size_t pos) const
    {
        throw ExprError(message, pos);
    }

    [[noreturn]] void unexpected(void) const
    {
        if (cur().kind == Tok::End) fail("unexpected end of expression", cur().pos);
        fail("unexpected '" + std::string(cur().text) + "'", cur().pos);
    }

    void expect(const std::string_view op)
    {
        if (not isOp(op)) this->unexpected();
        _i++;
    }

    NodePtr make(const Node::Kind kind, const std::size_t pos, std::vector<NodePtr> children = {})
    {
        auto node = std::make_unique<Node>();
        node->kind = kind;
        node->pos = pos;
        node->children = std::move(children);
        for (const auto &child : node->children) node->depth = std::max(node->depth, child->depth+1);
        if (node->depth > MaxTreeDepth) fail("expression is nested too deeply", pos);
        return node;
    }

    NodePtr make(const Node::Kind kind, const std::size_t pos, NodePtr a)
    {
        std::vector<NodePtr> children;
        children.push_back(std::move(a));
        return this->make(kind, pos, std::move(children));
    }

    NodePtr make(const Node::Kind kind, const std::size_t pos, NodePtr a, NodePtr b)
    {
        std::vector<NodePtr> children;
        children.push_back(std::move(a));
        children.push_back(std::move(b));
        return this->make(kind, pos, std::move(children));
    }

    //expr := or_expr ['if' or_expr 'else' expr]
    NodePtr expr(void)
    {
        Nest nest(*this);
        auto body = this->logic(true);
        if (not isKw("if")) return body;
        const auto pos = cur().pos;
        _i++;
        auto cond = this->logic(true);
        if (not isKw("else")) this->unexpected();
        _i++;
        std::vector<NodePtr> children;
        children.push_back(std::move(cond));
        children.push_back(std::move(body));
        children.push_back(this->expr());
        return this->make(Node::Kind::IfElse, pos, std::move(children));
    }

    //or_expr := and_expr ('or' and_expr)*, and_expr := not_expr ('and' not_expr)*
    //one function for both levels: fewer stack frames per nesting level
    NodePtr logic(const bool orLevel)
    {
        const std::string_view kw = orLevel? "or" : "and";
        auto first = orLevel? this->logic(false) : this->notExpr();
        if (not isKw(kw)) return first;
        const auto pos = cur().pos;
        std::vector<NodePtr> children;
        children.push_back(std::move(first));
        while (isKw(kw))
        {
            _i++;
            children.push_back(orLevel? this->logic(false) : this->notExpr());
        }
        return this->make(orLevel? Node::Kind::Or : Node::Kind::And, pos, std::move(children));
    }

    //not_expr := 'not' not_expr | compare
    NodePtr notExpr(void)
    {
        if (not isKw("not")) return this->compare();
        Nest nest(*this);
        const auto pos = cur().pos;
        _i++;
        return this->make(Node::Kind::Not, pos, this->notExpr());
    }

    //compare := binary (cmp_op binary)*
    NodePtr compare(void)
    {
        auto first = this->binary(1);
        const auto isCmp = [this]{return isAnyOp({"<", ">", "==", "!=", "<=", ">="});};
        if (not isCmp()) return first;
        const auto pos = cur().pos;
        std::vector<NodePtr> children;
        std::vector<std::string> ops;
        children.push_back(std::move(first));
        while (isCmp())
        {
            ops.emplace_back(cur().text);
            _i++;
            children.push_back(this->binary(1));
        }
        auto node = this->make(Node::Kind::Compare, pos, std::move(children));
        node->ops = std::move(ops);
        return node;
    }

    //! Precedence of the binary operator at the current token, 0 when it is not one
    int precedence(void) const
    {
        if (cur().kind != Tok::Op) return 0;
        const auto op = cur().text;
        if (op == "|") return 1;
        if (op == "^") return 2;
        if (op == "&") return 3;
        if (op == "<<" or op == ">>") return 4;
        if (op == "+" or op == "-") return 5;
        if (op == "*" or op == "/" or op == "//" or op == "%") return 6;
        return 0;
    }

    //left-associative binary operators by precedence climbing:
    //bit_or < bit_xor < bit_and < shift < arith < term, operands are unary
    NodePtr binary(const int minPrecedence)
    {
        auto left = this->unary();
        while (true)
        {
            const int prec = this->precedence();
            if (prec < minPrecedence or prec == 0) return left;
            const auto pos = cur().pos;
            std::string op(cur().text);
            _i++;
            left = this->make(Node::Kind::Binary, pos, std::move(left), this->binary(prec+1));
            left->op = std::move(op);
        }
    }

    //unary := ('-'|'+'|'~') unary | power
    NodePtr unary(void)
    {
        //-9223372036854775808 folds into one literal, its operand alone does not fit in int64
        const auto &next = _toks[_i+1 < _toks.size()? _i+1 : _i];
        const bool nextIsPower = _i+2 < _toks.size() and _toks[_i+2].kind == Tok::Op and _toks[_i+2].text == "**";
        if (isOp("-") and next.kind == Tok::Int and next.needsMinus and not nextIsPower)
        {
            auto node = this->make(Node::Kind::Literal, cur().pos);
            node->literal = next.literal;
            _i += 2;
            return node;
        }

        if (not isAnyOp({"-", "+", "~"})) return this->power();
        Nest nest(*this);
        const auto pos = cur().pos;
        std::string op(cur().text);
        _i++;
        auto node = this->make(Node::Kind::Unary, pos, this->unary());
        node->op = std::move(op);
        return node;
    }

    //power := postfix ['**' unary]
    NodePtr power(void)
    {
        auto base = this->postfix();
        if (not isOp("**")) return base;
        Nest nest(*this);
        const auto pos = cur().pos;
        _i++;
        auto node = this->make(Node::Kind::Binary, pos, std::move(base), this->unary());
        node->op = "**";
        return node;
    }

    //comma separated expressions up to the closing bracket, trailing comma allowed
    std::vector<NodePtr> items(const std::string_view close)
    {
        std::vector<NodePtr> out;
        while (not isOp(close))
        {
            out.push_back(this->expr());
            if (not isOp(",")) break;
            _i++;
        }
        this->expect(close);
        return out;
    }

    //postfix := atom ('(' args ')' | '[' expr ']')*
    NodePtr postfix(void)
    {
        auto node = this->atom();
        bool callable = node->kind == Node::Kind::Name;
        while (true)
        {
            if (isOp("("))
            {
                if (not callable) fail("only a function name can be called", cur().pos);
                _i++;
                auto call = this->make(Node::Kind::Call, node->pos, this->items(")"));
                call->name = std::move(node->name);
                node = std::move(call);
            }
            else if (isOp("["))
            {
                const auto pos = cur().pos;
                _i++;
                auto index = this->expr();
                this->expect("]");
                node = this->make(Node::Kind::Index, pos, std::move(node), std::move(index));
            }
            else return node;
            callable = false;
        }
    }

    NodePtr atom(void)
    {
        const auto &tok = cur();
        const auto pos = tok.pos;
        switch (tok.kind)
        {
        case Tok::Int:
            if (tok.needsMinus)
            {
                //2**63 without a unary minus: float like the larger decimal literals
                auto node = this->make(Node::Kind::Literal, pos);
                node->literal = Value(9223372036854775808.0);
                _i++;
                return node;
            }
            [[fallthrough]];
        case Tok::Float:
        case Tok::Imag:
        case Tok::String:
        {
            auto node = this->make(Node::Kind::Literal, pos);
            node->literal = tok.literal;
            _i++;
            return node;
        }
        case Tok::Name:
        {
            if (isKeyword(tok.text)) this->unexpected();
            auto node = this->make(Node::Kind::Name, pos);
            node->name = tok.text;
            _i++;
            return node;
        }
        case Tok::End: this->unexpected();
        case Tok::Op: break;
        }

        if (isOp("("))
        {
            _i++;
            auto node = this->expr();
            this->expect(")");
            return node;
        }
        if (isOp("["))
        {
            _i++;
            return this->make(Node::Kind::List, pos, this->items("]"));
        }
        if (isOp("{"))
        {
            _i++;
            std::vector<NodePtr> children;
            while (not isOp("}"))
            {
                children.push_back(this->expr());
                this->expect(":");
                children.push_back(this->expr());
                if (not isOp(",")) break;
                _i++;
            }
            this->expect("}");
            return this->make(Node::Kind::Dict, pos, std::move(children));
        }
        this->unexpected();
    }

    const std::vector<Token> _toks;
    std::size_t _i = 0;
    std::size_t _nesting = 0;
};

} //namespace

std::unique_ptr<Node> Pothos::Util::Expr::parse(const std::string_view expr)
{
    return Parser(expr).run();
}

std::string Pothos::Util::Expr::dump(const Node &node)
{
    std::string head;
    switch (node.kind)
    {
    case Node::Kind::Literal: return node.literal.repr();
    case Node::Kind::Name: return node.name;
    case Node::Kind::Unary: head = (node.op == "-")? "neg" : (node.op == "+")? "pos" : "inv"; break;
    case Node::Kind::Binary: head = node.op; break;
    case Node::Kind::Compare: if (node.ops.size() == 1) head = node.ops.front(); else head = "cmp"; break;
    case Node::Kind::And: head = "and"; break;
    case Node::Kind::Or: head = "or"; break;
    case Node::Kind::Not: head = "not"; break;
    case Node::Kind::IfElse: head = "if"; break;
    case Node::Kind::List: head = "list"; break;
    case Node::Kind::Dict: head = "dict"; break;
    case Node::Kind::Call: head = "call " + node.name; break;
    case Node::Kind::Index: head = "index"; break;
    }

    std::string out = "(" + head;
    for (std::size_t i = 0; i < node.children.size(); i++)
    {
        out += " " + dump(*node.children[i]);
        if (node.kind == Node::Kind::Compare and node.ops.size() > 1 and i < node.ops.size()) out += " " + node.ops[i];
    }
    return out + ")";
}
