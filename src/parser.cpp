/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <set>

#include "parser.h"

#if defined(_CHESS_) || defined(_XCHESS_)
#include "chess/chess.h"
#include "chess/chessbitboard.h"
#endif

#ifdef _XIANGQI_
#include "xq/xq.h"
#include "xq/xqbitboard.h"
#endif

#include "records.h"


using namespace ocgdb;

static const std::unordered_map<std::string, Operator> string2operatorMap {
    { "and", Operator::op_and }, { "&&", Operator::op_and }, { "or", Operator::op_or },
    { "||", Operator::op_or },   { "+", Operator::op_add },  { "-", Operator::op_sub },
    { "*", Operator::op_multi }, { "/", Operator::op_div },

    { "=", Operator::op_eq },    { "==", Operator::op_eq },  { "<", Operator::op_l },
    { "<=", Operator::op_le },   { ">", Operator::op_g },    { ">=", Operator::op_ge },
    { "<>", Operator::op_ne },   { "!=", Operator::op_ne },

    { "not", Operator::op_not }, { "!", Operator::op_not },
};

static Operator string2operator(const std::string& s)
{
    auto it = string2operatorMap.find(s);
    return it != string2operatorMap.end() ? it->second : Operator::none;
}

Node::~Node()
{
    for (auto&& bb : patternBitBoards) {
        delete bb;
    }
    patternBitBoards.clear();

    if (squareBB) {
        delete squareBB;
        squareBB = nullptr;
    }
}


Node::Node(banksia::ChessVariant variant, const LexWord& w) : variant(variant)
{
    assert(w.lex < Lex::no_node);
    string = w.string;
    if (w.lex == Lex::number) {
        nodeType = NodeType::number;
        number = atoi(w.string.c_str());
    } else if (w.lex >= Lex::operator_begin) {
        nodeType = NodeType::op;
        op = string2operator(string);
    }
}


static const std::string noteTypeStrings[] = { "none", "piece", "number", "op", "fen", "pattern", "keyword", "move", "line" };

std::string Node::toString() const
{
    std::string s = noteTypeStrings[static_cast<int>(nodeType)] + " " + string;

    //#ifdef _CHESS_
    if (nodeType == NodeType::piece && squareBB) {
        s += "\n" + squareBB->toString();
    }

    return s;
}

#ifdef _CHESS_
static int coordinate2pos(const char* s)
{
    auto c = s[0], d = s[1];
    if (c >= 'a' && c <= 'h' && d >= '1' && d <= '8') {
        auto pos = static_cast<int>('8' - d) * 8 + static_cast<int>(c - 'a');
        if (pos >= 0 && pos < 64) {
            return pos;
        }
    }

    return -1;
}
#endif

#ifdef _XIANGQI_
static int xqCoordinate2pos(const char* s)
{
    auto c = s[0], d = s[1];
    if (c >= 'a' && c <= 'i' && d >= '0' && d <= '9') {
        auto pos = static_cast<int>('9' - d) * 9 + static_cast<int>(c - 'a');
        if (pos >= 0 && pos < 90) {
            return pos;
        }
    }

    return -1;
}
#endif


int Node::selectSquare(const char* s)
{
#ifdef _CHESS_
    if (banksia::isChessFamily(variant)) {
        if (!s || !*s) return 0;

        auto len = strlen(s);
        if (len > 2) return -1;

        // column
        if (isalpha(*s)) {
            // row -> square
            if (isdigit(*(s + 1))) {
                auto pos = coordinate2pos(s);
                if (pos >= 0) {
                    if (!squareBB) {
                        squareBB = new banksia::ChessBB();
                    }
                    squareBB->setBit(pos); // |= banksia::ChessBoard::_posToBitboard[pos];
                    locSet.insert(pos);
                    return 1;
                }
                return -1;
            }

            if (len == 1) {
                auto c = static_cast<int>(*s - 'a');
                if (c >= 0 && c < 8) {
                    if (!squareBB) {
                        squareBB = new banksia::ChessBB();
                    }

                    for (auto i = 0; i < 8; ++i) {
                        auto x = i * 8 + c;
                        squareBB->setBit(x); // |= banksia::ChessBoard::_posToBitboard[x];
                        locSet.insert(x);
                    }
                    return 1;
                }
            }

            return -1;
        }

        // row
        if (*s >= '1' && *s <= '8') {
            auto r = static_cast<int>('8' - *s) * 8;

            if (!squareBB) {
                squareBB = new banksia::ChessBB();
            }
            for (auto i = 0; i < 8; ++i) {
                auto x = r + i;
                squareBB->setBit(x); // |= banksia::ChessBoard::_posToBitboard[x];
                locSet.insert(x);
            }
            return 1;
        }

        return -1;
    }
#endif

#ifdef _XIANGQI_
    if (banksia::isXqFamily(variant)) {
        if (!s || !*s) return 0;

        auto len = strlen(s);
        if (len > 2) return -1;

        // column
        if (isalpha(*s)) {
            // row -> square
            if (isdigit(*(s + 1))) {
                auto pos = xqCoordinate2pos(s);
                if (pos >= 0) {
                    if (!squareBB) {
                        squareBB = new banksia::XqBB();
                    }
                    squareBB->setBit(pos); // |= banksia::ChessBoard::_posToBitboard[pos];
                    locSet.insert(pos);
                    return 1;
                }
                return -1;
            }

            if (len == 1) {
                auto c = static_cast<int>(*s - 'a');
                if (c >= 0 && c < 9) {
                    if (!squareBB) {
                        squareBB = new banksia::XqBB();
                    }

                    for (auto i = 0; i < 9; ++i) {
                        auto x = i * 9 + c;
                        squareBB->setBit(x); // |= banksia::ChessBoard::_posToBitboard[x];
                        locSet.insert(x);
                    }
                    return 1;
                }
            }

            return -1;
        }

        // row
        if (*s >= '0' && *s <= '9') {
            auto r = static_cast<int>('9' - *s) * 9;

            if (!squareBB) {
                squareBB = new banksia::XqBB();
            }
            for (auto i = 0; i < 9; ++i) {
                auto x = r + i;
                squareBB->setBit(x); // |= banksia::ChessBoard::_posToBitboard[x];
                locSet.insert(x);
            }
            return 1;
        }
    }
#endif

    return -1;
}

int Node::selectSquare(const std::string& from, const std::string& to)
{
    assert(!from.empty() && !to.empty());

#ifdef _CHESS_
    if (banksia::isChessFamily(variant)) {
        if (from.size() == 2 && to.size() == 2) {
            auto fromPos = coordinate2pos(from.c_str());
            auto toPos = coordinate2pos(to.c_str());
            if (fromPos >= 0 && toPos >= 0 && fromPos != toPos) {
                if (fromPos > toPos) {
                    std::swap(fromPos, toPos);
                }

                if (!squareBB) {
                    squareBB = new banksia::ChessBB();
                }
                for (auto i = fromPos; i <= toPos; ++i) {
                    squareBB->setBit(i); // |= banksia::ChessBoard::_posToBitboard[i];
                    locSet.insert(i);
                }
                return 1;
            }
            return -1;
        }

        if (from.size() == 1 && to.size() == 1) {
            auto fch = from[0], tch = to[0];

            // column
            if (isalpha(fch) && isalpha(tch)) {
                if (fch > tch) std::swap(fch, tch);

                if (!squareBB) {
                    squareBB = new banksia::ChessBB();
                }
                for (auto t = fch; t <= tch; ++t) {
                    auto c = static_cast<int>(t - 'a');
                    for (auto i = 0; i < 8; ++i) {
                        auto x = i * 8 + c;
                        squareBB->setBit(x); // |= banksia::ChessBoard::_posToBitboard[x];
                        locSet.insert(x);
                    }
                }
                return 1;
            }

            if (isdigit(fch) && isdigit(tch)) {
                if (fch > tch) std::swap(fch, tch);

                if (!squareBB) {
                    squareBB = new banksia::ChessBB();
                }

                for (auto t = fch; t <= tch; ++t) {
                    auto r = static_cast<int>('8' - t) * 8;
                    for (auto i = 0; i < 8; ++i) {
                        auto x = r + i;
                        squareBB->setBit(x); // |= banksia::ChessBoard::_posToBitboard[x];
                        locSet.insert(x);
                    }
                }
                return 1;
            }
        }
        return -1;
    }

#endif

#ifdef _XIANGQI_
    if (banksia::isXqFamily(variant)) {
        if (from.size() == 2 && to.size() == 2) {
            auto fromPos = xqCoordinate2pos(from.c_str());
            auto toPos = xqCoordinate2pos(to.c_str());
            if (fromPos >= 0 && toPos >= 0 && fromPos != toPos) {
                if (fromPos > toPos) {
                    std::swap(fromPos, toPos);
                }

                if (!squareBB) {
                    squareBB = new banksia::XqBB();
                }
                for (auto i = fromPos; i <= toPos; ++i) {
                    squareBB->setBit(i); // |= banksia::ChessBoard::_posToBitboard[i];
                    locSet.insert(i);
                }
                return 1;
            }
            return -1;
        }

        if (from.size() == 1 && to.size() == 1) {
            auto fch = from[0], tch = to[0];

            // column
            if (isalpha(fch) && isalpha(tch)) {
                if (fch > tch) std::swap(fch, tch);

                if (!squareBB) {
                    squareBB = new banksia::XqBB();
                }
                for (auto t = fch; t <= tch; ++t) {
                    auto c = static_cast<int>(t - 'a');
                    for (auto i = 0; i < 9; ++i) {
                        auto x = i * 9 + c;
                        squareBB->setBit(x); // |= banksia::ChessBoard::_posToBitboard[x];
                        locSet.insert(x);
                    }
                }
                return 1;
            }

            if (isdigit(fch) && isdigit(tch)) {
                if (fch > tch) std::swap(fch, tch);

                if (!squareBB) {
                    squareBB = new banksia::XqBB();
                }

                for (auto t = fch; t <= tch; ++t) {
                    auto r = static_cast<int>('9' - t) * 9;
                    for (auto i = 0; i < 9; ++i) {
                        auto x = r + i;
                        squareBB->setBit(x); // |= banksia::ChessBoard::_posToBitboard[x];
                        locSet.insert(x);
                    }
                }
                return 1;
            }
        }
        return -1;
    }

#endif
    return -1;
}

bool Node::isValid() const
{
    switch (nodeType) {
        case NodeType::fen:
            return !fenHashSet.empty();

        case NodeType::op:
            if (op == Operator::op_not) {
                return lhs && !rhs && lhs->isValid();
            }
            return lhs && rhs && lhs->isValid() && rhs->isValid() && op < Operator::none;

        case NodeType::keyword:
            return keyword != Keyword::none;

        case NodeType::move:
            return !moveSpecVec.empty();

        case NodeType::line:
            return !lineMoves.empty();

        case NodeType::piece:
            return !lhs && !rhs;

        case NodeType::number:
            return !lhs && !rhs;

        case NodeType::pattern:
            return !patternBitBoards.empty() && patternTolerance >= 0;

        default:
            break;
    }

    return false;
}

int Node::evaluate(const PqlContext& ctx) const
{
    auto bb = ctx.bb;
    assert(bb);

    switch (nodeType) {
        case NodeType::fen: {
            assert(!fenHashSet.empty());
            auto hash = bb->getHash();
            return isInFenHashSet(hash) ? 1 : 0;
        }
        case NodeType::op: {
            assert(lhs);
            /// and, or, not: the right side only when it decides
            switch (op) {
                case Operator::op_and:
                    return lhs->evaluate(ctx) && rhs->evaluate(ctx) ? 1 : 0;
                case Operator::op_or:
                    return lhs->evaluate(ctx) || rhs->evaluate(ctx) ? 1 : 0;
                case Operator::op_not:
                    return lhs->evaluate(ctx) ? 0 : 1;
                default:
                    break;
            }

            assert(rhs);
            auto l = lhs->evaluate(ctx), r = rhs->evaluate(ctx);
            switch (op) {
                case Operator::op_add:
                    return l + r;
                case Operator::op_sub:
                    return l - r;
                case Operator::op_multi:
                    return l * r;
                case Operator::op_div:
                    return r != 0 ? l / r : 0; /// ?

                case Operator::op_eq:
                    return l == r ? 1 : 0;
                case Operator::op_l:
                    return l < r ? 1 : 0;
                case Operator::op_le:
                    return l <= r ? 1 : 0;
                case Operator::op_g:
                    return l > r ? 1 : 0;
                case Operator::op_ge:
                    return l >= r ? 1 : 0;
                case Operator::op_ne:
                    return l != r ? 1 : 0;

                default:
                    break;
            }
            break;
        }

        case NodeType::piece: {
            return bb->getPieceCount(string, squareBB);
        }

        break;

        case NodeType::number:
            assert(number == std::stoi(string));
            return number;

        case NodeType::pattern: {
            assert(!patternBitBoards.empty());
            for (auto&& patternVec : patternBitBoards) {
                if (patternOperand == PatternOperand::greaterthan) {
                    if (evaluate_pattern(bb, patternVec, PatternOperand::lessthan, patternTolerance)) {
                        return 1;
                    }
                }
                if (evaluate_pattern(patternVec, bb, patternOperand, patternTolerance)) {
                    return 1;
                }
            }
            break;
        }

        case NodeType::keyword:
            return evaluate_keyword(ctx);

        case NodeType::move:
            return evaluate_move(ctx) ? 1 : 0;

        case NodeType::line:
            return evaluate_line(ctx) ? 1 : 0;

        default:
            break;
    }

    return 0;
}

/// the move that led to the position: the last of the board's history
static const banksia::Hist* lastHist(const PqlContext& ctx)
{
    return ctx.board ? ctx.board->_getHistPointerAt(ctx.board->getHistListSize() - 1) : nullptr;
}

static bool isCapture(const banksia::Hist* hist)
{
    return !hist->cap.isEmpty() || (hist->flag & banksia::Hist::MoveFlag_CapEnpassant);
}

int Node::evaluate_keyword(const PqlContext& ctx) const
{
    auto board = ctx.board;
    if (!board) {
        return 0;
    }

    /// the unlocked (_) calls: the board belongs to the thread searching with it, and
    /// BoardCore::fromMoveList() holds its dataMutex while it calls back (isIncheck() deadlocked)
    switch (keyword) {
        case Keyword::ply:
            return board->getHistListSize();

        case Keyword::check:
            return board->_isIncheck(board->side) ? 1 : 0;

        case Keyword::mate: {
            if (!board->_isIncheck(board->side)) {
                return 0;
            }
            /// _genLegalOnly() makes and takes back each move: the board is as it was afterwards
            std::vector<banksia::MoveFull> moves;
            const_cast<banksia::BoardCore*>(board)->_genLegalOnly(moves, board->side);
            return moves.empty() ? 1 : 0;
        }

        case Keyword::capture: {
            auto hist = lastHist(ctx);
            return hist && isCapture(hist) ? 1 : 0;
        }

        case Keyword::promotion: {
            auto hist = lastHist(ctx);
            return hist && hist->move.promotion != banksia::EMPTY ? 1 : 0;
        }

        case Keyword::wtm:
            return board->side == banksia::Side::white ? 1 : 0;

        case Keyword::btm:
            return board->side == banksia::Side::black ? 1 : 0;

        default:
            break;
    }
    return 0;
}

bool Node::evaluate_move(const PqlContext& ctx) const
{
    auto hist = lastHist(ctx);
    if (!hist) {
        return false;
    }

    const auto& move = hist->move;
    const auto columnCnt = banksia::isXqFamily(variant) ? 9 : 8;

    for (auto&& spec : moveSpecVec) {
        if (spec.castle) {
#ifdef _CHESS_
            if (hist->castled
                == (spec.castle == 1 ? banksia::CastleRight_short : banksia::CastleRight_long)) {
                return true;
            }
#endif
            continue;
        }
        if ((spec.pieceType >= 0 && move.piece.type != spec.pieceType)
            || (spec.side != banksia::Side::none && move.piece.side != spec.side)
            || (spec.from >= 0 && move.from != spec.from)
            || (spec.fromCol >= 0 && move.from % columnCnt != spec.fromCol)
            || (spec.dest >= 0 && move.dest != spec.dest) || (spec.capture && !isCapture(hist))
            || (spec.promotion >= 0 && move.promotion != spec.promotion)) {
            continue;
        }
        return true;
    }
    return false;
}

/// true on the position reached by exactly the moves of line[...], from the variant's start position
bool Node::evaluate_line(const PqlContext& ctx) const
{
    auto board = ctx.board;
    if (!board || board->getHistListSize() != static_cast<int>(lineMoves.size())) {
        return false;
    }

    for (size_t i = 0; i < lineMoves.size(); ++i) {
        auto hist = board->_getHistPointerAt(static_cast<int>(i));
        const auto& m = lineMoves.at(i);
        if ((i == 0 && hist->hashKey != lineStartHash) || hist->move.from != m.from || hist->move.dest != m.dest
            || hist->move.promotion != m.promotion) {
            return false;
        }
    }
    return true;
}


bool Node::evaluate_pattern(const banksia::BitboardBase* bbSubVec, const banksia::BitboardBase* bbSuperVec,
                            PatternOperand operand, int tolerance) const
{
    return bbSubVec->evaluate_pattern(bbSuperVec, operand, tolerance);

}


////////////////////////////////////
/// The parser reads "a - b - c" as (a - b) - c and "A and B or C" as (A and B) or C, brackets
/// conditions, rejects whatever follows a complete query and never reads past the last token.
Parser::Parser() { }

Parser::~Parser()
{
    deleteTree();

    if (board) delete board;
    if (board2) delete board2;
}

void Parser::deleteTree()
{
    for (auto&& node : stages) {
        deleteTree(node);
    }
    stages.clear();
    root = nullptr;
}

void Parser::deleteTree(Node* node) const
{
    if (node) {
        deleteTree(node->lhs);
        deleteTree(node->rhs);
        delete node;
    }
}

static const std::string errorStrings[] = {
    "none",         "no input",       "wrong lexical",         "missing condition", "missing comparison",
    "missing term", "missing factor", "missing close bracket", "input invalid",     "unexpected",
    "unknown word", "wrong move",
};

std::string Parser::getErrorString(ParseError error)
{
    if (error >= ParseError::max) return "unknown";
    return errorStrings[static_cast<int>(error)];
}

std::string Parser::getErrorString() const
{
    auto s = getErrorString(error);
    if (error != ParseError::none && !errorNear.empty()) {
        s += " near \"" + errorNear + "\"";
    }
    return s;
}

void Parser::setError(ParseError e, size_t at)
{
    if (error == ParseError::none) {
        error = e;
        errorNear = at < lexVec.size() ? lexVec.at(at).string : "the end";
    }
}

void Parser::printError() const
{
    std::cerr << getErrorString() << std::endl;
}

void Parser::printTree() const
{
    for (size_t i = 0; i < stages.size(); ++i) {
        if (i) std::cout << "then" << std::endl;
        printTree(stages.at(i), "");
    }
}

void Parser::printTree(const Node* node, std::string prefix) const
{
    if (!node) return;

    std::cout << prefix << node->toString() << std::endl;

    prefix += "   ";
    printTree(node->lhs, prefix);
    printTree(node->rhs, prefix);
}

int Parser::evaluate(const PqlContext& ctx) const
{
    return root && root->evaluate(ctx) ? 1 : 0;
}

bool Parser::match(const PqlContext& ctx, int& stage) const
{
    if (stages.empty()) {
        return false;
    }
    if (stages.size() == 1) {
        return stages.front()->evaluate(ctx) != 0;
    }

    if (stage < 0) stage = 0;
    if (stage >= static_cast<int>(stages.size())) {
        return true;
    }
    if (stages.at(stage)->evaluate(ctx)) {
        ++stage;
        return stage == static_cast<int>(stages.size());
    }
    return false;
}

bool Parser::parse(banksia::ChessVariant _variant, const char* s)
{
    assert(s);
    deleteTree();
    error = ParseError::none;
    errorNear.clear();
    variant = _variant;

    if (board) delete board;
    if (board2) delete board2;

    board = ocgdb::createBoard(variant);
    board2 = ocgdb::createBoard(variant);

    lexVec = lexParse(s);
    if (error == ParseError::none && lexVec.empty()) {
        error = ParseError::noinput;
    }
    if (error != ParseError::none) {
        return false;
    }

    /// query := condition { "then" condition }
    size_t from = 0;
    while (error == ParseError::none) {
        auto node = parse_or(from);
        if (!node) {
            setError(ParseError::missing_condition, from);
            break;
        }
        stages.push_back(node);

        if (from < lexVec.size() && lexVec.at(from).lex == Lex::operator_then) {
            ++from;
            continue;
        }
        break;
    }

    if (error == ParseError::none && from < lexVec.size()) {
        setError(ParseError::unexpected, from);
    }

    if (error == ParseError::none) {
        for (auto&& node : stages) {
            if (!node->isValid()) {
                setError(ParseError::invalid, lexVec.size());
                break;
            }
        }
    }

    if (error != ParseError::none) {
        deleteTree();
        return false;
    }

    root = stages.front();
    return true;
}

Node* Parser::newOperator(const LexWord& word, Node* lhs, Node* rhs)
{
    auto op = new Node(variant, word);
    op->lhs = lhs;
    op->rhs = rhs;
    return op;
}

/// condition := and_condition { "or" and_condition }
Node* Parser::parse_or(size_t& from)
{
    auto node = parse_and(from);
    while (node && from < lexVec.size() && lexVec.at(from).lex == Lex::operator_or) {
        auto word = lexVec.at(from++);
        auto rhs = parse_and(from);
        if (!rhs) {
            setError(ParseError::missing_condition, from);
            deleteTree(node);
            return nullptr;
        }
        node = newOperator(word, node, rhs);
    }
    return node;
}

/// and_condition := not_condition { "and" not_condition }
Node* Parser::parse_and(size_t& from)
{
    auto node = parse_not(from);
    while (node && from < lexVec.size() && lexVec.at(from).lex == Lex::operator_and) {
        auto word = lexVec.at(from++);
        auto rhs = parse_not(from);
        if (!rhs) {
            setError(ParseError::missing_condition, from);
            deleteTree(node);
            return nullptr;
        }
        node = newOperator(word, node, rhs);
    }
    return node;
}

/// not_condition := "not" not_condition | comparison
Node* Parser::parse_not(size_t& from)
{
    if (from < lexVec.size() && lexVec.at(from).lex == Lex::operator_not) {
        auto word = lexVec.at(from++);
        auto operand = parse_not(from);
        if (!operand) {
            setError(ParseError::missing_condition, from);
            return nullptr;
        }
        return newOperator(word, operand, nullptr);
    }
    return parse_comparison(from);
}

/// comparison := expression { comparison_operator expression }
Node* Parser::parse_comparison(size_t& from)
{
    auto node = parse_sum(from);
    while (node && from < lexVec.size() && lexVec.at(from).lex == Lex::operator_comparison) {
        auto word = lexVec.at(from++);
        auto rhs = parse_sum(from);
        if (!rhs) {
            setError(ParseError::missing_term, from);
            deleteTree(node);
            return nullptr;
        }
        node = newOperator(word, node, rhs);
    }
    return node;
}

/// expression := term { ("+" | "-") term }
Node* Parser::parse_sum(size_t& from)
{
    auto node = parse_product(from);
    while (node && from < lexVec.size()
           && (lexVec.at(from).lex == Lex::operator_add || lexVec.at(from).lex == Lex::operator_sub)) {
        auto word = lexVec.at(from++);
        auto rhs = parse_product(from);
        if (!rhs) {
            setError(ParseError::missing_term, from);
            deleteTree(node);
            return nullptr;
        }
        node = newOperator(word, node, rhs);
    }
    return node;
}

/// term := factor { ("*" | "/") factor }
Node* Parser::parse_product(size_t& from)
{
    auto node = parse_primary(from);
    while (node && from < lexVec.size()
           && (lexVec.at(from).lex == Lex::operator_multi || lexVec.at(from).lex == Lex::operator_div)) {
        auto word = lexVec.at(from++);
        auto rhs = parse_primary(from);
        if (!rhs) {
            setError(ParseError::missing_factor, from);
            deleteTree(node);
            return nullptr;
        }
        node = newOperator(word, node, rhs);
    }
    return node;
}

static const std::unordered_map<std::string, Keyword> keywordMap {
    { "ply", Keyword::ply },          { "check", Keyword::check }, { "mate", Keyword::mate },
    { "capture", Keyword::capture },  { "promotion", Keyword::promotion },
    { "wtm", Keyword::wtm },          { "btm", Keyword::btm },
};

/// factor := number | piece | fen | pattern | keyword | move[...] | line[...] | "(" condition ")"
Node* Parser::parse_primary(size_t& from)
{
    if (error != ParseError::none || from >= lexVec.size()) {
        return nullptr;
    }

    auto word = lexVec.at(from);
    switch (word.lex) {
        case Lex::number:
            ++from;
            return new Node(variant, word);

        case Lex::fen:
            return parse_fenstring(from);

        case Lex::bracket:
            if (word.string == "(") {
                ++from;
                auto node = parse_or(from);
                if (!node) {
                    setError(ParseError::missing_condition, from);
                    return nullptr;
                }
                if (from < lexVec.size() && lexVec.at(from).string == ")") {
                    ++from;
                    return node;
                }
                setError(ParseError::missing_close, from);
                deleteTree(node);
                return nullptr;
            }
            if (word.string == "{") {
                return parse_pattern(from);
            }
            return nullptr;

        case Lex::string: {
            if (word.string == "fen") {
                return parse_fenclause(from);
            }
            if (word.string == "move") {
                return parse_move(from);
            }
            if (word.string == "line") {
                return parse_line(from);
            }
            auto it = keywordMap.find(word.string);
            if (it != keywordMap.end()) {
                ++from;
                auto node = new Node(variant);
                node->nodeType = NodeType::keyword;
                node->string = word.string;
                node->keyword = it->second;
                return node;
            }

            auto node = parse_piece(from);
            if (!node) {
                setError(ParseError::unknown_word, from);
            }
            return node;
        }

        default:
            break;
    }
    return nullptr;
}

/// fen[fen, fen, ...]
Node* Parser::parse_fenclause(size_t& from)
{
    assert(from < lexVec.size() && lexVec.at(from).string == "fen");
    auto start = from;

    if (from + 2 >= lexVec.size() || lexVec.at(from + 1).string != "[") {
        setError(ParseError::invalid, from);
        return nullptr;
    }

    auto node = new Node(variant);
    node->nodeType = NodeType::fen;

    for (from += 2;; ++from) {
        if (from >= lexVec.size()) {
            setError(ParseError::missing_close, from);
            delete node;
            return nullptr;
        }
        auto lex = lexVec.at(from);
        if (lex.lex == Lex::fen) {
            board->newGame(lex.string);
            node->fenHashSet.insert(board->hashKey);
            continue;
        }
        if (lex.lex == Lex::comma) {
            continue;
        }
        if (lex.string == "]") {
            from++;
            break;
        }
        setError(ParseError::wrong_lexical, from);
        delete node;
        return nullptr;
    }

    if (node->fenHashSet.empty()) {
        setError(ParseError::invalid, start);
        delete node;
        return nullptr;
    }
    return node;
}

Node* Parser::parse_fenstring(size_t& from)
{
    if (from < lexVec.size() && lexVec.at(from).lex == Lex::fen) {
        auto lex = lexVec.at(from);
        from++;

        board->newGame(lex.string);
        auto node = new Node(variant);
        node->nodeType = NodeType::fen;
        node->fenHashSet.insert(board->hashKey);
        return node;
    }
    return nullptr;
}

const std::map<std::string, PatternOperand> patternOperandMap { { "=", PatternOperand::equal },
                                                                { "==", PatternOperand::equal },
                                                                { "<", PatternOperand::lessthan },
                                                                { ">", PatternOperand::greaterthan },
                                                                { "#", PatternOperand::shift } };

/// pattern := "{" pattern_term { "," pattern_term } [ "," operand [ "," tolerance ] ] "}"
Node* Parser::parse_pattern(size_t& from)
{
    assert(from < lexVec.size() && lexVec.at(from).string == "{");
    auto start = from;

    auto patternOperand = PatternOperand::equal;
    auto patternOperandDelta = 0;
    board->_clear();

    auto closed = false;
    for (++from; from < lexVec.size();) {
        auto lex = lexVec.at(from);
        if (lex.lex == Lex::fen) {
            board2->newGame(lex.string);

            for (auto i = 0; i < board2->size(); ++i) {
                auto piece = board2->getPiece(i);
                if (piece.isEmpty()) continue;
                board->_setPiece(i, piece);
            }
            ++from;
            continue;
        }

        if (lex.lex == Lex::comma) {
            ++from;
            continue;
        }
        if (lex.string == "}") {
            from++;
            closed = true;
            break;
        }

        auto it = patternOperandMap.find(lex.string);
        if (it != patternOperandMap.end()) {
            patternOperand = it->second;
            ++from;
            continue;
        }

        if (lex.lex == Lex::number) {
            patternOperandDelta = std::stoi(lex.string);
            ++from;
            continue;
        }

        if (lex.lex == Lex::string) {
            auto node = parse_piece(from);
            if (!node) {
                setError(ParseError::unknown_word, from);
                return nullptr;
            }
            /// a pattern places pieces: "white"/"black" or a piece without squares places nothing
            if (node->locSet.empty() || node->pieceSide == banksia::Side::none) {
                delete node;
                setError(ParseError::invalid, from - 1);
                return nullptr;
            }
            banksia::Piece piece(node->pieceType, node->pieceSide);
            for (auto&& sq : node->locSet) {
                board->_setPiece(sq, piece);
            }
            delete node;
            if (error != ParseError::none) {
                return nullptr;
            }
            continue;
        }

        setError(ParseError::wrong_lexical, from);
        return nullptr;
    }

    if (!closed) {
        setError(ParseError::missing_close, from);
        return nullptr;
    }

    if (board->_isEmpty()) {
        setError(ParseError::invalid, start);
        return nullptr;
    }

    auto node = new Node(variant);
    node->nodeType = NodeType::pattern;

    node->patternBitBoards.push_back(board->posToBitboards());

    node->patternOperand = patternOperand;
    node->patternTolerance = patternOperandDelta;

    // shift
    if (patternOperand == PatternOperand::shift) {
#ifdef _CHESS_
        if (banksia::isChessFamily(variant)) {
            banksia::ChessBoard::pattern_shift(node->patternBitBoards);
        }
#endif
    }

    return node;
}

/// "a1" (chess) or "a0" (xiangqi) as a board position; -1 when it is not one
static int squareToPos(banksia::ChessVariant variant, const char* s)
{
#ifdef _CHESS_
    if (banksia::isChessFamily(variant)) {
        return coordinate2pos(s);
    }
#endif
#ifdef _XIANGQI_
    if (banksia::isXqFamily(variant)) {
        return xqCoordinate2pos(s);
    }
#endif
    (void)variant;
    (void)s;
    return -1;
}

static std::string trimmed(const std::string& s)
{
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

/// One move pattern:
///   [piece] [from square | from file] ["x"] [to square] ["=" promotion | promotion]   or O-O, O-O-O
/// The piece letter's case is the side, as everywhere in PQL (N a White knight, n a Black one); a
/// letter followed by a digit is a square. Everything left out matches anything: "N" any White
/// knight move, "x" any capture, "e4" any move to e4, "Pe7e8=Q" a White promotion to a queen.
bool Parser::parse_movespec(const std::string& str, MoveSpec& spec) const
{
    auto s = trimmed(str);
    if (s.empty()) {
        return false;
    }

    if (banksia::isChessFamily(variant)) {
        if (s == "O-O" || s == "0-0") {
            spec.castle = 1;
            return true;
        }
        if (s == "O-O-O" || s == "0-0-0") {
            spec.castle = 2;
            return true;
        }
    }

    const char* pieceLetters = banksia::isXqFamily(variant) ? "KAERCHP" : "KQRBNP";
    const int columnCnt = banksia::isXqFamily(variant) ? 9 : 8;
    const auto len = s.size();

    auto squareAt = [&](size_t k) -> int {
        if (k + 1 < len && isalpha(s[k]) && isdigit(s[k + 1])) {
            return squareToPos(variant, s.c_str() + k);
        }
        return -1;
    };
    auto pieceTypeOf = [&](char ch) -> int {
        auto p = strchr(pieceLetters, toupper(ch));
        return p && *p ? banksia::KING + static_cast<int>(p - pieceLetters) : -1;
    };
    auto isColumn = [&](char ch) { return ch >= 'a' && ch < 'a' + columnCnt; };

    size_t k = 0;
    if (isalpha(s[0]) && squareAt(0) < 0) {
        auto type = pieceTypeOf(s[0]);
        if (type >= 0) {
            spec.pieceType = type;
            spec.side = isupper(s[0]) ? banksia::Side::white : banksia::Side::black;
            k = 1;
        }
    }

    std::vector<int> squares;
    while (k < len) {
        auto ch = s[k];
        auto pos = squareAt(k);
        if (pos >= 0) {
            squares.push_back(pos);
            k += 2;
        } else if (ch == 'x' || ch == ':') {
            spec.capture = true;
            ++k;
        } else if (ch == '-' || ch == '+' || ch == '#' || ch == '!' || ch == '?') {
            ++k;
        } else if (ch == '=') {
            if (k + 1 >= len || (spec.promotion = pieceTypeOf(s[k + 1])) < 0) {
                return false;
            }
            k += 2;
        } else if (isColumn(ch) && squares.empty() && spec.fromCol < 0) {
            spec.fromCol = ch - 'a';
            ++k;
        } else if (!squares.empty() && pieceTypeOf(ch) >= 0) {
            spec.promotion = pieceTypeOf(ch);
            ++k;
        } else {
            return false;
        }
    }

    if (squares.size() > 2) {
        return false;
    }
    if (squares.size() == 2) {
        spec.from = squares.front();
    }
    if (!squares.empty()) {
        spec.dest = squares.back();
    }
    return true;
}

/// move[spec, spec, ...]: the move that led to the position matches one of them
Node* Parser::parse_move(size_t& from)
{
    if (from + 1 >= lexVec.size() || lexVec.at(from + 1).lex != Lex::raw) {
        setError(ParseError::invalid, from);
        return nullptr;
    }
    auto raw = lexVec.at(from + 1).string;
    from += 2;

    auto node = new Node(variant);
    node->nodeType = NodeType::move;
    node->string = "move[" + raw + "]";

    std::stringstream ss(raw);
    std::string item;
    while (std::getline(ss, item, ',')) {
        MoveSpec spec;
        if (!parse_movespec(item, spec)) {
            delete node;
            setError(ParseError::wrong_move, from - 1);
            errorNear = trimmed(item);
            return nullptr;
        }
        node->moveSpecVec.push_back(spec);
    }

    if (node->moveSpecVec.empty()) {
        delete node;
        setError(ParseError::invalid, from - 1);
        return nullptr;
    }
    return node;
}

/// line[1. e4 e5 2. Nf3]: the position reached by exactly these moves from the start position
Node* Parser::parse_line(size_t& from)
{
    if (from + 1 >= lexVec.size() || lexVec.at(from + 1).lex != Lex::raw) {
        setError(ParseError::invalid, from);
        return nullptr;
    }
    auto raw = lexVec.at(from + 1).string;
    from += 2;

    auto node = new Node(variant);
    node->nodeType = NodeType::line;
    node->string = "line[" + raw + "]";

    board->newGame();
    node->lineStartHash = board->hashKey;

    std::stringstream ss(raw);
    std::string token;
    while (ss >> token) {
        /// move numbers ("1.", "1...", "12.e4") and a result are not moves
        auto k = token.find_first_not_of("0123456789");
        if (k != std::string::npos && k > 0 && token[k] == '.') {
            token = token.substr(token.find_first_not_of('.', k) == std::string::npos
                                         ? token.size()
                                         : token.find_first_not_of('.', k));
        }
        if (token.empty() || token == "*" || token == "1-0" || token == "0-1" || token == "1/2-1/2") {
            continue;
        }

        auto move = board->moveFromString_san(token, banksia::moveFormat);
        if (!board->isValid(move)) {
            move = board->moveFromString_coordinate(token, banksia::moveFormat);
        }
        if (!board->isValid(move) || !board->checkMake(move.from, move.dest, move.promotion)) {
            delete node;
            setError(ParseError::wrong_move, from - 1);
            errorNear = token;
            return nullptr;
        }
        node->lineMoves.push_back(banksia::Move(move.from, move.dest, move.promotion));
    }

    if (node->lineMoves.empty()) {
        delete node;
        setError(ParseError::invalid, from - 1);
        return nullptr;
    }
    return node;
}

Node* Parser::parse_piece(size_t& from)
{
    assert(from <= lexVec.size());

    auto node = parse_piecename(from);
    if (!node) return nullptr;

    if (from < lexVec.size()) {
        parse_squareset(node, from);
    }
    return node;
}

void Parser::parse_squareset(Node* node, size_t& from)
{
    assert(node);
    if (from >= lexVec.size() || lexVec.at(from).string != "[") {
        return;
    }
    ++from;
    while (from < lexVec.size()) {
        auto at = from;
        auto word = lexVec.at(from++);
        if (word.lex == Lex::comma) {
            continue;
        }
        if (word.string == "]") {
            return;
        }

        /// a range: a-c, 1-3, a2-a4
        if (from + 1 < lexVec.size() && lexVec.at(from).string == "-") {
            from++;
            if (node->selectSquare(word.string, lexVec.at(from++).string) < 0) {
                setError(ParseError::wrong_lexical, at);
                return;
            }
            continue;
        }
        if ((word.lex != Lex::string && word.lex != Lex::number) || node->selectSquare(word.string.c_str()) < 0) {
            setError(ParseError::wrong_lexical, at);
            return;
        }
    }

    setError(ParseError::missing_close, from);
}

Node* Parser::parse_piecename(size_t& from)
{
    assert(from <= lexVec.size());

    const char* pieceNames = nullptr;

#ifdef _CHESS_
    if (banksia::isChessFamily(variant)) {
        pieceNames = "KQRBNPkqrbnpw"; // w-b for white/black/bishop
    }
#endif

#ifdef _XIANGQI_
    if (banksia::isXqFamily(variant)) {
        pieceNames = "KAERCHPkaerchpwb"; // w-b for white/black
    }
#endif

    if (!pieceNames || from >= lexVec.size()) {
        return nullptr;
    }

    auto word = lexVec.at(from);
    if (word.lex == Lex::string) {
        assert(!word.string.empty());
        auto ch = word.string.at(0);
        size_t len = 1;
        auto t = strchr(pieceNames, ch);
        if (t) {
            /// "white", "black": all pieces of a side; a lone "w" (or, in xiangqi, "b") is no piece
            auto isWhite = word.string.compare(0, 5, "white") == 0;
            auto isBlack = word.string.compare(0, 5, "black") == 0;
            if ((ch == 'w' && !isWhite) || (ch == 'b' && !isBlack && banksia::isXqFamily(variant))) {
                return nullptr;
            }

            auto node = new Node(variant);
            node->nodeType = NodeType::piece;
            node->string = ch;
            if (isWhite || isBlack) {
                node->string = isWhite ? "white" : "black";
                len = 5;
            } else {
                /// KQRBNP / KAERCHP, in the order of the piece types (xiangqi has 7: this was % 6,
                /// which made every xiangqi pattern piece after the advisor the wrong type)
                auto typeCnt = banksia::isXqFamily(variant) ? 7 : 6;
                node->pieceType = banksia::KING + ((t - pieceNames) % typeCnt);
                node->pieceSide = isupper(ch) ? banksia::Side::white : banksia::Side::black;
            }

            if (len < word.string.size()) {
                if (node->selectSquare(word.string.c_str() + len) < 0) {
                    setError(ParseError::wrong_lexical, from);
                }
            }

            ++from;
            return node;
        }
    }

    return nullptr;
}

/// Splits a query into words: a FEN ends with its own fields, so "<fen> and q = 1" is not one long
/// FEN; "8-2" is a subtraction, not a FEN; "&&", "||", "!" and the words "not", "then" are operators;
/// move[...] and line[...] keep their text whole (Lex::raw); an unknown character is an error, not
/// skipped.
std::vector<LexWord> Parser::lexParse(const char* s)
{
    assert(s && error == ParseError::none);

    std::vector<LexWord> words;
    const std::string str(s);
    const auto n = str.size();

    auto push = [&](Lex lex, const std::string& text) {
        LexWord word;
        word.lex = lex;
        word.string = text;
        words.push_back(word);
    };
    auto fail = [&](size_t at) {
        error = ParseError::wrong_lexical;
        errorNear = str.substr(at, 10);
    };

    static const char* comparisonChars = "=<>!";

    for (size_t i = 0; i < n && error == ParseError::none;) {
        auto c = str[i];
        if (isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }

        if (isalnum(static_cast<unsigned char>(c))) {
            auto j = i;
            while (j < n && (isalnum(static_cast<unsigned char>(str[j])) || str[j] == '/')) ++j;
            auto word = str.substr(i, j - i);

            /// a FEN: its board has '/'; then up to five more fields that look like FEN fields
            if (word.find('/') != std::string::npos) {
                std::string fen = word;
                for (auto field = 1; field < 6; ++field) {
                    auto k = j;
                    while (k < n && str[k] == ' ') ++k;
                    auto e = k;
                    while (e < n && (isalnum(static_cast<unsigned char>(str[e])) || str[e] == '-')) ++e;
                    if (e == k) break;
                    auto f = str.substr(k, e - k);
                    auto ok = false;
                    switch (field) {
                        case 1:
                            ok = f == "w" || f == "b" || f == "r";
                            break;
                        case 2:
                            ok = f.find_first_not_of("KQkqABCDEFGHabcdefgh-") == std::string::npos;
                            break;
                        case 3:
                            ok = f == "-" || (f.size() == 2 && isalpha(f[0]) && isdigit(f[1]));
                            break;
                        default:
                            ok = f.find_first_not_of("0123456789") == std::string::npos;
                            break;
                    }
                    if (!ok) break;
                    fen += " " + f;
                    j = e;
                }
                push(Lex::fen, fen);
                i = j;
                continue;
            }

            if (isdigit(static_cast<unsigned char>(c))) {
                auto e = i;
                while (e < n && isdigit(static_cast<unsigned char>(str[e]))) ++e;
                if (e < n && isalpha(static_cast<unsigned char>(str[e]))) {
                    fail(i); // "4white"
                    break;
                }
                push(Lex::number, str.substr(i, e - i));
                i = e;
                continue;
            }

            /// move[...], line[...]: the text up to "]" is read by the parser
            if ((word == "move" || word == "line") && j < n && str[j] == '[') {
                auto close = str.find(']', j + 1);
                if (close == std::string::npos) {
                    error = ParseError::missing_close;
                    errorNear = word + "[";
                    break;
                }
                push(Lex::string, word);
                push(Lex::raw, str.substr(j + 1, close - j - 1));
                i = close + 1;
                continue;
            }

            auto lex = Lex::string;
            if (word == "and") {
                lex = Lex::operator_and;
            } else if (word == "or") {
                lex = Lex::operator_or;
            } else if (word == "not") {
                lex = Lex::operator_not;
            } else if (word == "then") {
                lex = Lex::operator_then;
            }
            push(lex, word);
            i = j;
            continue;
        }

        if (strchr(comparisonChars, c)) {
            auto j = i;
            while (j < n && strchr(comparisonChars, str[j])) ++j;
            auto text = str.substr(i, j - i);
            if (text == "!") {
                push(Lex::operator_not, text);
            } else if (string2operator(text) >= Operator::none) {
                fail(i);
                break;
            } else {
                push(Lex::operator_comparison, text);
            }
            i = j;
            continue;
        }

        if ((c == '&' || c == '|') && i + 1 < n && str[i + 1] == c) {
            push(c == '&' ? Lex::operator_and : Lex::operator_or, str.substr(i, 2));
            i += 2;
            continue;
        }

        auto lex = Lex::none;
        switch (c) {
            case '+':
                lex = Lex::operator_add;
                break;
            case '-':
                lex = Lex::operator_sub;
                break;
            case '*':
                lex = Lex::operator_multi;
                break;
            case '/':
                lex = Lex::operator_div;
                break;
            case '#':
                lex = Lex::operator_pattern_shift;
                break;
            case '{':
            case '}':
            case '(':
            case ')':
            case '[':
            case ']':
                lex = Lex::bracket;
                break;
            case ',':
                lex = Lex::comma;
                break;
            default:
                break;
        }
        if (lex == Lex::none) {
            fail(i);
            break;
        }
        push(lex, std::string(1, c));
        ++i;
    }

    return words;
}
