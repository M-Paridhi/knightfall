#include "board.h"
#include "move.h"
#include <sstream>
#include <iostream>
#include <cassert>
#include <random>
#include <cmath>

Board::Board() { initZobrist(); setStartingPosition(); }

void Board::initZobrist() {
    std::mt19937_64 rng(0xDEADBEEFCAFEBABEULL);
    for (int c=0;c<2;++c) for (int pt=0;pt<6;++pt) for (int s=0;s<64;++s)
        zobristTable[c][pt][s] = rng();
    zobristBlackToMove = rng();
    for (int cr=0;cr<16;++cr) zobristCastling[cr] = rng();
    for (int f=0;f<8;++f)    zobristEnPassant[f]  = rng();
}

void Board::putPiece(Piece p, Square s) {
    assert(pieceOnSquare[s]==NO_PIECE);
    Color c=colorOf(p); PieceType pt=typeOf(p);
    setBit(pieceBB[c][pt],s); setBit(byColor[c],s); setBit(byType[pt],s);
    pieceOnSquare[s]=p;
    state.zobristKey ^= zobristTable[c][pt][s];
}

void Board::removePiece(Square s) {
    Piece p=pieceOnSquare[s]; assert(p!=NO_PIECE);
    Color c=colorOf(p); PieceType pt=typeOf(p);
    clearBit(pieceBB[c][pt],s); clearBit(byColor[c],s); clearBit(byType[pt],s);
    pieceOnSquare[s]=NO_PIECE;
    state.zobristKey ^= zobristTable[c][pt][s];
}

void Board::movePiece(Square from, Square to) {
    Piece p=pieceOnSquare[from]; assert(p!=NO_PIECE); assert(pieceOnSquare[to]==NO_PIECE);
    Color c=colorOf(p); PieceType pt=typeOf(p);
    Bitboard mask=SquareBB[from]|SquareBB[to];
    pieceBB[c][pt]^=mask; byColor[c]^=mask; byType[pt]^=mask;
    pieceOnSquare[from]=NO_PIECE; pieceOnSquare[to]=p;
    state.zobristKey ^= zobristTable[c][pt][from]^zobristTable[c][pt][to];
}

void Board::putPieceSilent(Piece p, Square s) {
    assert(p!=NO_PIECE); assert(pieceOnSquare[s]==NO_PIECE);
    Color c=colorOf(p); PieceType pt=typeOf(p);
    setBit(pieceBB[c][pt],s); setBit(byColor[c],s); setBit(byType[pt],s);
    pieceOnSquare[s]=p;
}

void Board::removePieceSilent(Square s) {
    Piece p=pieceOnSquare[s]; if(p==NO_PIECE) return;
    Color c=colorOf(p); PieceType pt=typeOf(p);
    clearBit(pieceBB[c][pt],s); clearBit(byColor[c],s); clearBit(byType[pt],s);
    pieceOnSquare[s]=NO_PIECE;
}

void Board::movePieceSilent(Square from, Square to) {
    Piece p=pieceOnSquare[from]; assert(p!=NO_PIECE); assert(pieceOnSquare[to]==NO_PIECE);
    Color c=colorOf(p); PieceType pt=typeOf(p);
    Bitboard mask=SquareBB[from]|SquareBB[to];
    pieceBB[c][pt]^=mask; byColor[c]^=mask; byType[pt]^=mask;
    pieceOnSquare[from]=NO_PIECE; pieceOnSquare[to]=p;
}

void Board::setFromFEN(const std::string& fen) {
    for(int c=0;c<2;++c) for(int pt=0;pt<6;++pt) pieceBB[c][pt]=0;
    for(int c=0;c<2;++c) byColor[c]=0;
    for(int pt=0;pt<6;++pt) byType[pt]=0;
    for(int s=0;s<64;++s) pieceOnSquare[s]=NO_PIECE;
    state=StateInfo{}; state.enPassantSquare=NO_SQUARE;
    state.castlingRights=NO_CASTLING; state.capturedPiece=NO_PIECE;
    state.zobristKey=0ULL; historyPly=0;

    std::istringstream ss(fen); std::string token;
    ss>>token;
    int rank=7,file=0;
    for(char ch:token){
        if(ch=='/'){--rank;file=0;}
        else if(ch>='1'&&ch<='8'){file+=(ch-'0');}
        else{
            const std::string chars="PNBRQKpnbrqk";
            int idx=(int)chars.find(ch);
            if(idx!=(int)std::string::npos){
                Color c=(idx<6)?WHITE:BLACK; PieceType pt=PieceType(idx%6);
                putPiece(Piece(c*6+pt),makeSquare(File(file),Rank(rank))); ++file;
            }
        }
    }
    ss>>token; stm=(token=="w")?WHITE:BLACK;
    if(stm==BLACK) state.zobristKey^=zobristBlackToMove;
    ss>>token;
    for(char ch:token){
        switch(ch){
            case 'K': state.castlingRights|=WHITE_OO;  break;
            case 'Q': state.castlingRights|=WHITE_OOO; break;
            case 'k': state.castlingRights|=BLACK_OO;  break;
            case 'q': state.castlingRights|=BLACK_OOO; break;
        }
    }
    state.zobristKey^=zobristCastling[state.castlingRights];
    ss>>token;
    if(token!="-"){
        File f=File(token[0]-'a'); Rank r=Rank(token[1]-'1');
        state.enPassantSquare=makeSquare(f,r);
        state.zobristKey^=zobristEnPassant[f];
    }
    if(ss>>token) state.halfmoveClock=std::stoi(token);
    if(ss>>token) fullmoveNum=std::stoi(token);
}

void Board::setStartingPosition() {
    setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

std::string Board::toFEN() const {
    std::ostringstream oss;
    const std::string pc="PNBRQKpnbrqk";
    for(int r=7;r>=0;--r){
        int empty=0;
        for(int f=0;f<8;++f){
            Piece p=pieceOnSquare[makeSquare(File(f),Rank(r))];
            if(p==NO_PIECE){++empty;}
            else{if(empty){oss<<empty;empty=0;}oss<<pc[p];}
        }
        if(empty)oss<<empty; if(r>0)oss<<'/';
    }
    oss<<' '<<(stm==WHITE?'w':'b')<<' ';
    bool any=false;
    if(state.castlingRights&WHITE_OO) {oss<<'K';any=true;}
    if(state.castlingRights&WHITE_OOO){oss<<'Q';any=true;}
    if(state.castlingRights&BLACK_OO) {oss<<'k';any=true;}
    if(state.castlingRights&BLACK_OOO){oss<<'q';any=true;}
    if(!any)oss<<'-';
    oss<<' ';
    if(state.enPassantSquare!=NO_SQUARE)
        oss<<char('a'+fileOf(state.enPassantSquare))<<char('1'+rankOf(state.enPassantSquare));
    else oss<<'-';
    oss<<' '<<state.halfmoveClock<<' '<<fullmoveNum;
    return oss.str();
}

static const int castlingMask[64]={
    ~WHITE_OOO,15,15,15,~(WHITE_OO|WHITE_OOO),15,15,~WHITE_OO,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    ~BLACK_OOO,15,15,15,~(BLACK_OO|BLACK_OOO),15,15,~BLACK_OO
};

void Board::makeMove(Move m) {
    Square    from     = moveFrom(m);
    Square    to       = moveTo(m);
    MoveType  type     = moveType(m);
    PieceType promoted = movePromotion(m);
    Piece     moved    = pieceOnSquare[from];
    Piece     captured = (type==EN_PASSANT) ? NO_PIECE : pieceOnSquare[to];

    // ── KEY FIX: set capturedPiece BEFORE saving to history ──
    // unmakeMove reads capturedPiece from the restored history entry.
    // If we save history first and set capturedPiece after, the history
    // entry has the OLD capturedPiece, not the current one.
    if (type == EN_PASSANT)
        state.capturedPiece = Piece(stm == WHITE ? BLACK_PAWN : WHITE_PAWN);
    else
        state.capturedPiece = captured;

    state.zobristKey ^= zobristCastling[state.castlingRights];
    history[historyPly++] = state;  // save AFTER capturedPiece is set

    // ── Handle capture ──
    if (captured != NO_PIECE)
        removePiece(to);

    // ── Execute ──
    if (type == NORMAL) {
        movePiece(from, to);
    } else if (type == CASTLING) {
        movePiece(from, to);
        Square rookFrom, rookTo;
        if      (to==G1){rookFrom=H1;rookTo=F1;}
        else if (to==C1){rookFrom=A1;rookTo=D1;}
        else if (to==G8){rookFrom=H8;rookTo=F8;}
        else            {rookFrom=A8;rookTo=D8;}
        movePiece(rookFrom, rookTo);
    } else if (type == EN_PASSANT) {
        movePiece(from, to);
        removePiece((stm==WHITE) ? Square(to-8) : Square(to+8));
    } else { // PROMOTION
        removePiece(from);
        putPiece(Piece(stm*6+promoted), to);
    }

    // ── En passant ──
    if (state.enPassantSquare!=NO_SQUARE)
        state.zobristKey^=zobristEnPassant[fileOf(state.enPassantSquare)];
    state.enPassantSquare=NO_SQUARE;
    if (typeOf(moved)==PAWN && std::abs(to-from)==16) {
        state.enPassantSquare=Square((from+to)/2);
        state.zobristKey^=zobristEnPassant[fileOf(state.enPassantSquare)];
    }

    // ── Castling rights ──
    state.castlingRights &= castlingMask[from] & castlingMask[to];
    state.zobristKey ^= zobristCastling[state.castlingRights];

    // ── Halfmove clock ──
    if (typeOf(moved)==PAWN || captured!=NO_PIECE || type==EN_PASSANT || type==PROMOTION)
        state.halfmoveClock=0;
    else
        ++state.halfmoveClock;

    // ── Flip side ──
    stm=~stm;
    state.zobristKey^=zobristBlackToMove;
    if(stm==WHITE) ++fullmoveNum;
}

void Board::unmakeMove(Move m) {
    stm=~stm;
    if(stm==BLACK) --fullmoveNum;
    state=history[--historyPly];

    Square   from=moveFrom(m), to=moveTo(m);
    MoveType type=moveType(m);

    if(type==NORMAL){
        movePieceSilent(to,from);
        if(state.capturedPiece!=NO_PIECE) putPieceSilent(state.capturedPiece,to);
    } else if(type==CASTLING){
        movePieceSilent(to,from);
        Square rookFrom,rookTo;
        if      (to==G1){rookFrom=H1;rookTo=F1;}
        else if (to==C1){rookFrom=A1;rookTo=D1;}
        else if (to==G8){rookFrom=H8;rookTo=F8;}
        else            {rookFrom=A8;rookTo=D8;}
        movePieceSilent(rookTo,rookFrom);
    } else if(type==EN_PASSANT){
        movePieceSilent(to,from);
        Square capPawn=(stm==WHITE)?Square(to-8):Square(to+8);
        putPieceSilent(state.capturedPiece,capPawn);
    } else { // PROMOTION
        removePieceSilent(to);
        putPieceSilent(Piece(stm*6+PAWN),from);
        if(state.capturedPiece!=NO_PIECE) putPieceSilent(state.capturedPiece,to);
    }
}

void Board::print() const {
    const std::string pc="PNBRQKpnbrqk.";
    std::cout<<"\n";
    for(int r=7;r>=0;--r){
        std::cout<<(r+1)<<" | ";
        for(int f=0;f<8;++f)
            std::cout<<pc[pieceOnSquare[makeSquare(File(f),Rank(r))]]<<' ';
        std::cout<<"\n";
    }
    std::cout<<"    a b c d e f g h\n";
    std::cout<<"FEN: "<<toFEN()<<"\n";
    std::cout<<"Hash: "<<std::hex<<state.zobristKey<<std::dec<<"\n\n";
}
