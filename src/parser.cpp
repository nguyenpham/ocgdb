/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <map>
#include <set>

#include "parser.h"
#include "board/chess.h"
#include "board/base.h"

static int popCount(uint64_t x)
{
   int count = 0;
   while (x) {
       count++;
       x &= x - 1; // reset LS1B
   }
   return count;
}

using namespace ocgdb;

static const std::unordered_map<std::string, Operator> string2operatorMap{
    {"and", Operator::op_and},
    {"&&", Operator::op_and},
    {"or", Operator::op_or},
    {"||", Operator::op_or},
    {"+", Operator::op_add},
    {"-", Operator::op_sub },
    {"*", Operator::op_multi},
    {"/", Operator::op_div},

    {"=", Operator::op_eq},
    {"==", Operator::op_eq},
    {"<", Operator::op_l},
    {"<=", Operator::op_le},
    {">", Operator::op_g},
    {">=", Operator::op_ge},
    {"<>", Operator::op_ne},
    {"!=", Operator::op_ne},
};

static Operator string2operator(const std::string& s)
{
    auto it = string2operatorMap.find(s);
    return it != string2operatorMap.end() ? it->second : Operator::none;
}

Node::Node(const LexWord& w)
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


static const std::string noteTypeStrings[] = {
    "none", "piece", "number", "op"
};

std::string Node::toString() const
{
    std::string s =
        noteTypeStrings[static_cast<int>(nodeType)]
        + " " + string
        ;
    
    if (nodeType == NodeType::piece && hassquareset) {
        s += "\n" + bslib::ChessBoard::bitboard2string(squareset);
    }
    
    return s;
}

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

int Node::selectSquare(const char* s)
{
    if (!s || !*s) return 0;
    
    auto len = strlen(s);
    if (len > 2) return -1;
    
    // column
    if (isalpha(*s)) {
        // row -> square
        if (isdigit(*(s + 1))) {
            auto pos = coordinate2pos(s);
            if (pos >= 0) {
                squareset |= bslib::ChessBoard::_posToBitboard[pos];
                hassquareset = true;
                return 1;
            }
            return -1;
        }
        
        if (len == 1) {
            auto c = static_cast<int>(*s - 'a');
            if (c >= 0 && c < 8) {
                
                for(auto i = 0; i < 8; ++i) {
                    auto x = i * 8 + c;
                    squareset |= bslib::ChessBoard::_posToBitboard[x];
                }
                hassquareset = true;
                return 1;
            }
        }
        
        return -1;
    }

    // row
    if (*s >= '1' && *s <= '8') {
        auto r = static_cast<int>('8' - *s) * 8;

        for(auto i = 0; i < 8; ++i) {
            auto x = r + i;
            squareset |= bslib::ChessBoard::_posToBitboard[x];
        }
        hassquareset = true;
        return 1;
    }

    return -1;
}

int Node::selectSquare(const std::string& from, const std::string& to)
{
    assert(!from.empty() && !to.empty());
    
    if (from.size() == 2 && to.size() == 2) {
        auto fromPos = coordinate2pos(from.c_str());
        auto toPos = coordinate2pos(to.c_str());
        if (fromPos >= 0 && toPos >= 0 && fromPos != toPos) {
            if (fromPos > toPos) {
                std::swap(fromPos, toPos);
            }
            
            for(auto i = fromPos; i <= toPos; ++i) {
                squareset |= bslib::ChessBoard::_posToBitboard[i];
            }
            hassquareset = true;
            return 1;
        }
        return -1;
    }

    if (from.size() == 1 && to.size() == 1) {
        auto fch = from[0], tch = to[0];

        // column
        if (isalpha(fch) && isalpha(tch)) {
            if (fch > tch) std::swap(fch, tch);
            
            for(auto t = fch; t <= tch; ++t) {
                auto c = static_cast<int>(t - 'a');
                for(auto i = 0; i < 8; ++i) {
                    auto x = i * 8 + c;
                    squareset |= bslib::ChessBoard::_posToBitboard[x];
                }
            }
            hassquareset = true;
            return 1;
        }

        if (isdigit(fch) && isdigit(tch)) {
            if (fch > tch) std::swap(fch, tch);
            
            for(auto t = fch; t <= tch; ++t) {
                auto r = static_cast<int>('8' - t) * 8;
                for(auto i = 0; i < 8; ++i) {
                    auto x = r + i;
                    squareset |= bslib::ChessBoard::_posToBitboard[x];
                }
            }
            hassquareset = true;
            return 1;
        }
    }
    
    return -1;
}


bool Node::isValid() const
{
    switch (nodeType) {
        case NodeType::fen:
            return !fenHashSet.empty();

        case NodeType::op:
            return lhs && rhs && lhs->isValid() && rhs->isValid() && op < Operator::none;
            
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

int Node::evaluate(const std::vector<uint64_t>& bitboardVec) const
{
    switch (nodeType) {
        case NodeType::fen:
        {
            assert(!fenHashSet.empty());
            auto hash = bitboardVec[static_cast<int>(bslib::BBIdx::hash)];
            return isInFenHashSet(hash) ? 1 : 0;
        }
        case NodeType::op:
        {
            assert(lhs && rhs);
            auto l = lhs->evaluate(bitboardVec), r = rhs->evaluate(bitboardVec);
            switch (op) {
                case Operator::op_and:
                    return (l && r) ? 1 : 0;
                case Operator::op_or:
                    return (l || r) ? 1 : 0;
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
            
        case NodeType::piece:
            assert(!lhs && !rhs);
            
            int64_t bb;
            switch (string.at(0)) {
                case 'w':
                    assert(string == "white");
                    bb = bitboardVec[static_cast<int>(bslib::BBIdx::white)];
                    break;

                case 'K':
                    bb = bitboardVec[static_cast<int>(bslib::BBIdx::white)] & bitboardVec[static_cast<int>(bslib::BBIdx::kings)];
                    break;

                case 'Q':
                    bb = bitboardVec[static_cast<int>(bslib::BBIdx::white)] & bitboardVec[static_cast<int>(bslib::BBIdx::queens)];
                    break;
                case 'R':
                    bb = bitboardVec[static_cast<int>(bslib::BBIdx::white)] & bitboardVec[static_cast<int>(bslib::BBIdx::rooks)];
                    break;

                case 'B':
                    bb = bitboardVec[static_cast<int>(bslib::BBIdx::white)] & bitboardVec[static_cast<int>(bslib::BBIdx::bishops)];
                    break;
                case 'N':
                    bb = bitboardVec[static_cast<int>(bslib::BBIdx::white)] & bitboardVec[static_cast<int>(bslib::BBIdx::knights)];
                    break;
                case 'P':
                    bb = bitboardVec[static_cast<int>(bslib::BBIdx::white)] & bitboardVec[static_cast<int>(bslib::BBIdx::pawns)];
                    break;

                case 'k':
                    bb = bitboardVec[static_cast<int>(bslib::BBIdx::black)] & bitboardVec[static_cast<int>(bslib::BBIdx::kings)];
                    break;

                case 'q':
                    bb = bitboardVec[static_cast<int>(bslib::BBIdx::black)] & bitboardVec[static_cast<int>(bslib::BBIdx::queens)];
                    break;
                    
                case 'r':
                    bb = bitboardVec[static_cast<int>(bslib::BBIdx::black)] & bitboardVec[static_cast<int>(bslib::BBIdx::rooks)];
                    break;

                case 'b':
                    bb = bitboardVec[static_cast<int>(bslib::BBIdx::black)];
                    if (string == "b") {
                        bb &= bitboardVec[static_cast<int>(bslib::BBIdx::bishops)];
                    } else {
                        assert(string == "black");
                    }
                    break;
                case 'n':
                    bb = bitboardVec[static_cast<int>(bslib::BBIdx::black)] & bitboardVec[static_cast<int>(bslib::BBIdx::knights)];
                    break;
                case 'p':
                    bb = bitboardVec[static_cast<int>(bslib::BBIdx::black)] & bitboardVec[static_cast<int>(bslib::BBIdx::pawns)];
                    break;

                default:
                    bb = 0;
                    break;
            }
            
            if (hassquareset) {
                bb &= squareset;
            }
            return popCount(bb);
            
        case NodeType::number:
            assert(number == std::atoi(string.c_str()));
            return number;
            
        case NodeType::pattern:
        {
            assert(!patternBitBoards.empty());
            auto i = 0;
            for(auto && patternVec : patternBitBoards) {
                if (patternOperand == PatternOperand::greaterthan) {
                    if (evaluate_pattern(bitboardVec, patternVec, PatternOperand::lessthan, patternTolerance)) {
                        return 1;
                    }
                }
                if (evaluate_pattern(patternVec, bitboardVec, patternOperand, patternTolerance)) {
                    return 1;
                }
                i++;
            }
            break;
        }

        default:
            break;
    }
    
    return 0;
}

bool Node::evaluate_pattern(const std::vector<uint64_t>& bbSubVec, const std::vector<uint64_t>& bbSuperVec, PatternOperand operand, int tolerance) const
{
    auto blackSb = bbSubVec[static_cast<int>(bslib::BBIdx::black)], whiteSb = bbSubVec[static_cast<int>(bslib::BBIdx::white)];
    auto blackSp = bbSuperVec[static_cast<int>(bslib::BBIdx::black)], whiteSp = bbSuperVec[static_cast<int>(bslib::BBIdx::white)];

    uint64_t blackDif, whiteDif;

    if (operand == PatternOperand::equal) {
        blackDif = (blackSb | blackSp) & ~(blackSb & blackSp);
        whiteDif = (whiteSb | whiteSp) & ~(whiteSb & whiteSp);
    } else {
        blackDif = blackSb & ~(blackSb & blackSp);
        whiteDif = whiteSb & ~(whiteSb & whiteSp);
    }

    auto cnt = popCount(blackDif | whiteDif);

    if (cnt > tolerance) return false;

    // check further, each position
    uint64_t d = 0;
    for(auto idx = static_cast<int>(bslib::BBIdx::kings); idx < static_cast<int>(bslib::BBIdx::max); ++idx) {
        auto sb = bbSubVec[idx], sp = bbSuperVec[idx];

        uint64_t dif;

        if (operand == PatternOperand::equal) {
            dif = (sb | sp) & ~(sb & sp);
        } else {
            dif = sb & ~(sb & sp);
        }

        d |= dif;
    }

    return popCount(d) <= tolerance;
}

void Node::pattern_shift()
{
    assert(patternBitBoards.size() == 1);

    auto v = patternBitBoards.at(0);

    while (pattern_shift_up(v)) {
        patternBitBoards.push_back(v);
        auto v2 = v;
        while (pattern_shift_left(v2)) {
            patternBitBoards.push_back(v2);
        }
        v2 = v;
        while (pattern_shift_right(v2)) {
            patternBitBoards.push_back(v2);
        }
    }

    v = patternBitBoards.at(0);
    while (pattern_shift_down(v)) {
        patternBitBoards.push_back(v);
        auto v2 = v;
        while (pattern_shift_left(v2)) {
            patternBitBoards.push_back(v2);
        }
        v2 = v;
        while (pattern_shift_right(v2)) {
            patternBitBoards.push_back(v2);
        }
    }
}

bool Node::pattern_shift_up(std::vector<uint64_t>& v)
{
    assert(!v.empty());
    auto bb = v[static_cast<int>(bslib::BBIdx::black)] | v[static_cast<int>(bslib::BBIdx::white)];
    assert(bb);

    if (bb & bslib::ChessBoard::bb_edge_top)
        return false;

    for(int i = static_cast<int>(bslib::BBIdx::black); i < static_cast<int>(bslib::BBIdx::max); i++) {
        v[i] <<= 8;

        // Pawn can't go to the top line
        if (i == static_cast<int>(bslib::BBIdx::pawns) && (v[i] & bslib::ChessBoard::bb_edge_top)) {
            return false;
        }
    }

    v[static_cast<int>(bslib::BBIdx::blackkingsquare)] += 8;
    v[static_cast<int>(bslib::BBIdx::whitekingsquare)] += 8;

    return true;
}

bool Node::pattern_shift_down(std::vector<uint64_t>& v)
{
    assert(!v.empty());
    auto bb = v[static_cast<int>(bslib::BBIdx::black)] | v[static_cast<int>(bslib::BBIdx::white)];
    assert(bb);

    if (bb & bslib::ChessBoard::bb_edge_bottom)
        return false;

    for(int i = static_cast<int>(bslib::BBIdx::black); i < static_cast<int>(bslib::BBIdx::max); i++) {
        v[i] >>= 8;

        // Pawn can't go to the bottom line
        if (i == static_cast<int>(bslib::BBIdx::pawns) && (v[i] & bslib::ChessBoard::bb_edge_bottom)) {
            return false;
        }
    }

    v[static_cast<int>(bslib::BBIdx::blackkingsquare)] -= 8;
    v[static_cast<int>(bslib::BBIdx::whitekingsquare)] -= 8;

    return true;
}

bool Node::pattern_shift_left(std::vector<uint64_t>& v)
{
    assert(!v.empty());
    auto bb = v[static_cast<int>(bslib::BBIdx::black)] | v[static_cast<int>(bslib::BBIdx::white)];
    assert(bb);

    if (bb & bslib::ChessBoard::bb_edge_left)
        return false;

    for(int i = static_cast<int>(bslib::BBIdx::black); i < static_cast<int>(bslib::BBIdx::max); i++) {
        v[i] >>= 1;
    }

    v[static_cast<int>(bslib::BBIdx::blackkingsquare)]--;
    v[static_cast<int>(bslib::BBIdx::whitekingsquare)]--;
    return true;
}

bool Node::pattern_shift_right(std::vector<uint64_t>& v)
{
    assert(!v.empty());
    auto bb = v[static_cast<int>(bslib::BBIdx::black)] | v[static_cast<int>(bslib::BBIdx::white)];
    assert(bb);

    if (bb & bslib::ChessBoard::bb_edge_right)
        return false;

    for(int i = static_cast<int>(bslib::BBIdx::black); i < static_cast<int>(bslib::BBIdx::max); i++) {
        v[i] <<= 1;
    }

    v[static_cast<int>(bslib::BBIdx::blackkingsquare)]++;
    v[static_cast<int>(bslib::BBIdx::whitekingsquare)]++;
    return true;
}

////////////////////////////////////
Parser::Parser()
{
}

Parser::~Parser()
{
    deleteTree();

    if (board) delete board;
    if (board2) delete board2;
}

void Parser::deleteTree()
{
    deleteTree(root);
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
    "none",
    "no input",
    "wrong lexical",
    "missing condition",
    "missing comparison",
    "missing term",
    "missing factor",
    "missing close bracket",
    "input invalid",
};

std::string Parser::getErrorString(ParseError error)
{
    if (error >= ParseError::max) return "unknown";
    return errorStrings[static_cast<int>(error)];
}

std::string Parser::getErrorString() const
{
    return getErrorString(error);
}

void Parser::printError() const
{
    std::cerr << getErrorString() << std::endl;
}

void Parser::printTree() const
{
    printTree(root, "");
}

void Parser::printTree(const Node* node, std::string prefix) const
{
    if (!node) return;
    
    std::cout << prefix << node->toString() << std::endl;
    
    prefix += "   ";
    printTree(node->lhs, prefix);
    printTree(node->rhs, prefix);
}

int Parser::evaluate(const std::vector<uint64_t>& bitboardVec) const
{
    return root && root->evaluate(bitboardVec);
}

bool Parser::parse(bslib::ChessVariant _variant, const char* s)
{
    assert(s);
    deleteTree();
    error = ParseError::none;
    variant = _variant;

    if (board) delete board;
    if (board2) delete board2;

    board = bslib::Funcs::createBoard(variant);
    board2 = bslib::Funcs::createBoard(variant);


    lexVec = lexParse(s);
    if (error == ParseError::none && lexVec.empty()) {
        error = ParseError::noinput;
    }
    if (error != ParseError::none) {
        return false;
    }

    Node* r = nullptr, *w = nullptr;
    
    // there is only one fen clause
    size_t from = 0;
    while (from < lexVec.size()) {
        auto node = parse_condition(from);
        if (!node) {
            node = parse_fenstring(from);
        }
        if (!node) {
            node = parse_pattern(from);
        }
        if (!node) {
            break;
        }
        
        auto stop = from >= lexVec.size();
        if (!stop) {
            auto lex = lexVec.at(from).lex;
            stop = lex != Lex::operator_and && lex != Lex::operator_or;
        }
        if (stop) {
            if (r) {
                assert(w && !w->rhs);
                w->rhs = node;
            } else {
                r = node;
            }
            break;
        }
        
        auto word = lexVec.at(from);
        assert(word.lex == Lex::operator_and || word.lex == Lex::operator_or);

        auto op = new Node(word);
        op->lhs = node;

        if (!r) {
            r = op;
        }
        if (w) {
            assert(!w->rhs);
            w->rhs = op;
        }
        w = op;
        ++from;
        if (from >= lexVec.size()) {
            // wrong
            error = ParseError::missing_condition;
            break;
        }
    }

    root = r;
    
    if (!root) {
        return false;
    }
    
    if (error == ParseError::none) {
        if (!root || !root->isValid()) {
            error = ParseError::invalid;
        }
    }
    
    return error == ParseError::none;
}

//
Node* Parser::parse_fenclause(size_t& from)
{
    assert(from <= lexVec.size());
    if (lexVec.at(from).string != "fen") {
        return nullptr;
    }

    if (lexVec.size() < 4 || lexVec.at(from + 1).string != "[" || lexVec.at(from + 2).lex != Lex::fen) {
        error = ParseError::invalid;
        return nullptr;
    }
    
    auto node = new Node();
    node->nodeType = NodeType::fen;

    auto board = bslib::Funcs::createBoard(bslib::ChessVariant::standard);
    for(from += 2; from < lexVec.size(); ++from) {
        auto lex = lexVec.at(from);
        if (lex.lex == Lex::fen) {
            board->newGame(lex.string);
            node->fenHashSet.insert(board->hashKey);
            continue;
        }

        if (lex.string == ",") {
            continue;
        }
        if (lex.string == "]") {
            from++;
            break;
        }
    }

    delete board;
    return node;
}

Node* Parser::parse_fenstring(size_t& from)
{
    if (from < lexVec.size() && lexVec.at(from).lex == Lex::fen) {
        auto lex = lexVec.at(from);
        from++;

        board->newGame(lex.string);
        auto node = new Node();
        node->nodeType = NodeType::fen;
        node->fenHashSet.insert(board->hashKey);
        return node;
    }
    return nullptr;
}

const std::map<std::string, ocgdb::PatternOperand> patternOperandMap {
    {"=", PatternOperand::equal},
    { "==", PatternOperand::equal},
    { "<", PatternOperand::lessthan},
    { ">", PatternOperand::greaterthan},
    { "#", PatternOperand::shift}
};

Node* Parser::parse_pattern(size_t& from)
{
    assert(from <= lexVec.size());
    if (lexVec.at(from).string != "{") {
        return nullptr;
    }

    auto patternOperand = PatternOperand::equal;
    auto patternOperandDelta = 0;
    board->_clear();

    auto cnt = 0;
    for(++from; from < lexVec.size(); ++from) {
        auto lex = lexVec.at(from);
        if (lex.lex == Lex::fen) {
            board2->newGame(lex.string);

            for(auto i = 0; i < board2->size(); ++i) {
                auto piece = board2->getPiece(i);
                if (piece.isEmpty()) continue;
                board->_setPiece(i, piece);
            }
            continue;
        }

        if (lex.string == ",") {
            cnt++;
            continue;
        }
        if (lex.string == "}") {
            from++;
            break;
        }

        auto it = patternOperandMap.find(lex.string);
        if (it != patternOperandMap.end()) {
            patternOperand = it->second;
            continue;
        }

        auto node = parse_piece(from);
        if (node) {
            if (!node->locSet.empty() && node->pieceSide != bslib::Side::none) {
                bslib::Piece piece(node->pieceType, node->pieceSide);
                for(auto && sq : node->locSet) {
                    board->_setPiece(sq, piece);
                }
            }

            delete node;
            continue;
        }

        if (std::isdigit(lex.string.at(0))) {
            patternOperandDelta = std::stoi(lex.string);
            continue;
        }
    }

    if (board->_isEmpty()) {
        return nullptr;
    }

    auto node = new Node();
    node->nodeType = NodeType::pattern;
    node->patternBitBoards.push_back(board->posToBitboards());

    node->patternOperand = patternOperand;
    node->patternTolerance = patternOperandDelta;

    // shift
    if (patternOperand == PatternOperand::shift) {
        node->pattern_shift();
    }

    return node;
}

Node* Parser::parse_condition(size_t& from)
{
    assert(from <= lexVec.size());
    
    auto node = parse_fenclause(from);
    if (node) {
        return node;
    }
    
    if (error != ParseError::none) {
        return nullptr;
    }

    Node* r = nullptr, *w = nullptr;
    while (from < lexVec.size()) {
        auto node = parse_expression(from);
        if (!node) {
            break;
        }
        auto stop = from >= lexVec.size();
        if (!stop) {
            auto lex = lexVec.at(from).lex;
            stop = lex != Lex::operator_comparison;
        }
        if (stop) {
            if (r) {
                assert(w && !w->rhs);
                w->rhs = node;
            } else {
                r = node;
            }
            break;
        }
        
        auto word = lexVec.at(from);
        assert(word.lex == Lex::operator_comparison);
        
        auto op = new Node(word);
        op->lhs = node;

        if (!r) {
            r = op;
        }
        if (w) {
            assert(!w->rhs);
            w->rhs = op;
        }
        w = op;
        ++from;
        if (from >= lexVec.size()) {
            // wrong
            error = ParseError::missing_op_comparison;
            break;
        }
    }

    return r;
}

Node* Parser::parse_expression(size_t& from)
{
    assert(from <= lexVec.size());

    Node* r = nullptr, *w = nullptr;

    while (from < lexVec.size()) {
        auto node = parse_term(from);
        if (!node) {
            break;
        }
        
        auto stop = from >= lexVec.size();
        if (!stop) {
            auto lex = lexVec.at(from).lex;
            stop = lex != Lex::operator_add && lex != Lex::operator_sub;
        }
        if (stop) {
            if (r) {
                assert(w && !w->rhs);
                w->rhs = node;
            } else {
                r = node;
            }
            break;
        }

        auto word = lexVec.at(from);
        assert(word.lex == Lex::operator_add || word.lex == Lex::operator_sub);
        
        auto op = new Node(word);
        op->lhs = node;

        if (!r) {
            r = op;
        }
        if (w) {
            assert(!w->rhs);
            w->rhs = op;
        }
        w = op;

        ++from;
        if (from >= lexVec.size()) {
            // wrong
            error = ParseError::missing_term;
            break;
        }
    }

    return r;
}

Node* Parser::parse_term(size_t& from)
{
    assert(from <= lexVec.size());
    Node* r = nullptr, *w = nullptr;
    while (from < lexVec.size()) {
        auto node = parse_factor(from);
        if (!node) {
            break;
        }
        
        auto stop = from >= lexVec.size();
        if (!stop) {
            auto lex = lexVec.at(from).lex;
            stop = lex != Lex::operator_multi && lex != Lex::operator_div;
        }
        if (stop) {
            if (r) {
                assert(w && !w->rhs);
                w->rhs = node;
            } else {
                r = node;
            }
            break;
        }

        auto word = lexVec.at(from);
        assert(word.lex == Lex::operator_multi || word.lex == Lex::operator_div);

        auto op = new Node(word);
        op->lhs = node;

        if (!r) {
            r = op;
        }
        if (w) {
            assert(!w->rhs);
            w->rhs = op;
        }
        w = op;
        ++from;
        if (from >= lexVec.size()) {
            // wrong
            error = ParseError::missing_factor;
            break;
        }
    }

    return r;
}

Node* Parser::parse_factor(size_t& from)
{
    assert(from <= lexVec.size());

    auto word = lexVec.at(from);
    if (word.lex == Lex::number) {
        ++from;
        return new Node(word);
    }

    if (word.string == "(") {
        assert(word.lex == Lex::bracket);
        ++from;
        auto node = parse_expression(from);
        auto word2 = lexVec.at(from);
        if (word2.string == ")") {
            assert(word2.lex == Lex::bracket);
            from++;
            return node;
        } else {
            delete node;
            // wrong
            error = ParseError::missing_close;
        }
    }
    else
    if (word.lex == Lex::string) {
        return parse_piece(from);
    }

    // wrong
    return nullptr;
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
    auto word = lexVec.at(from);
    if (word.string != "[") {
        return;
    }
    assert(word.lex == Lex::bracket);
    ++from;
    while(from < lexVec.size()) {
        auto word = lexVec.at(from++);
        if (word.lex == Lex::comma) {
            continue;
        }
        if (word.string == "]") {
            assert(word.lex == Lex::bracket);
            return;
        }
        
        if (from + 1 < lexVec.size() && lexVec.at(from).string == "-") {
            from++;
            if (node->selectSquare(word.string, lexVec.at(from++).string) < 0) {
                error = ParseError::wrong_lexical;
                return;
            }
            continue;
        }
        if (word.lex == Lex::string || word.lex == Lex::number) {
            if (node->selectSquare(word.string.c_str()) < 0) {
                error = ParseError::wrong_lexical;
                return;
            }
        }
    }

    error = ParseError::missing_close;
}

// R rc b8
Node* Parser::parse_piecename(size_t& from)
{
    assert(from <= lexVec.size());

    auto word = lexVec.at(from);
    if (word.lex == Lex::string) {
        assert(!word.string.empty());
        auto ch = word.string.at(0);
        auto len = 1;
        if (strchr("KQRBNPkqrbnpw", ch)) {
            auto node = new Node;
            node->nodeType = NodeType::piece;
            node->string = ch;
            if (ch == 'w') {
                if (memcmp(word.string.c_str(), "white", 5) == 0) {
                    node->string = "white";
                    len = 5;
                }
            } else
            if (ch == 'b') {
                if (memcmp(word.string.c_str(), "black", 5) == 0) {
                    node->string = "black";
                    len = 5;
                }
            }
            
            if (len < word.string.size()) {
                if (node->selectSquare(word.string.c_str() + len) < 0) {
                    error = ParseError::wrong_lexical;
                }
            }

            ++from;
            return node;
        }
    }

    return nullptr;
}

std::vector<LexWord> Parser::lexParse(const char* s)
{
    assert(s && error == ParseError::none);

    enum class State {
        none, text, number, comparison, fen
    };

    std::vector<LexWord> words;

    std::string text;
    auto state = State::none;
    auto ok = true;

    static const char* comparisonChars = "=<>!";
    for(auto p = s; ok && error == ParseError::none; ++p) {
        switch (state) {
            case State::none:
            {
                if (!*p) {
                    ok = false;
                    break;
                }

                if (isalpha(*p)) {
                    state = State::text;
                    text = *p;
                    break;
                }
                if (isdigit(*p)) {
                    state = State::number;
                    text = *p;
                    break;
                }

                if (strchr(comparisonChars, *p)) {
                    state = State::comparison;
                    text = *p;
                    break;
                }

                LexWord word;
                switch (*p) {
                    case '+':
                        word.lex = Lex::operator_add;
                        break;
                    case '-':
                        word.lex = Lex::operator_sub;
                        break;
                    case '*':
                        word.lex = Lex::operator_multi;
                        break;
                    case '/':
                        word.lex = Lex::operator_div;
                        break;
                    case '#':
                        word.lex = Lex::operator_pattern_shift;
                        break;
                    case '{':
                    case '}':
                    case '(':
                    case ')':
                    case '[':
                    case ']':
                        word.lex = Lex::bracket;
                        break;
                    case ',':
                        word.lex = Lex::comma;
                        break;

                    default:
                        if (!*p) {
                            ok = false;
                            break;
                        }
                        break;
                }

                if (word.lex != Lex::none) {
                    word.string += *p;
                    words.push_back(word);
                    break;
                }
                break;
            }

            case State::text:
                if (!isalnum(*p)) {
                    if (*p == '/') { //  || *p == '-'
                        state = State::fen;
                        --p;
                        break;
                    }

                    state = State::none;
                    --p;

                    LexWord word;
                    word.lex = Lex::string;
                    word.string = text;

                    if (text == "and") {
                        word.lex = Lex::operator_and;
                    } else if (text == "or") {
                        word.lex = Lex::operator_or;
                    }
                    words.push_back(word);
                    break;
                }
                text += *p;
                break;

            case State::fen:
                if (!isalnum(*p) && *p != '/' && *p != '-' && *p != ' ') {
                    state = State::none;
                    --p;

                    LexWord word;
                    word.lex = Lex::fen;
                    word.string = text;

                    words.push_back(word);
                    break;
                }
                text += *p;
                break;

            case State::comparison:
            {
                if (strchr(comparisonChars, *p)) {
                    text += *p;
                    break;
                }

                state = State::none;
                --p;

                if (string2operator(text) >= Operator::none) {
                    // wrong
                    error = ParseError::wrong_lexical;
                    break;
                }
                LexWord word;
                word.lex = Lex::operator_comparison;
                word.string = text;
                words.push_back(word);
                break;
            }

            case State::number:
                if (!isdigit(*p)) {
                    if (isalnum(*p) || *p == '/' || *p == '-') {
                        state = State::fen;
                        --p;
                        break;
                    }

                    state = State::none;

                    if (isalpha(*p)) {
                        // wrong
                        error = ParseError::wrong_lexical;
                        break;
                    }
                    --p;

                    LexWord word;
                    word.lex = Lex::number;
                    word.string = text;
                    words.push_back(word);
                    break;
                }
                text += *p;
                break;

            default:
                break;
        }
    }

    return words;
}
