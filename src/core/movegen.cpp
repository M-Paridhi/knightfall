#include "movegen.h"

static void addMoves(MoveList& list, Square from, Bitboard targets) {
    while (targets) { Square to=popLsb(targets); list.add(encodeMove(from,to)); }
}
static void addPromotions(MoveList& list, Square from, Square to) {
    list.add(encodeMove(from,to,PROMOTION,QUEEN));
    list.add(encodeMove(from,to,PROMOTION,ROOK));
    list.add(encodeMove(from,to,PROMOTION,BISHOP));
    list.add(encodeMove(from,to,PROMOTION,KNIGHT));
}
static bool isLegal(Board& board, Move m) {
    board.makeMove(m);
    Color moved=~board.sideToMove();
    Bitboard kb=board.pieces(moved,KING);
    bool legal=kb && !isAttacked(board,board.kingSquare(moved),board.sideToMove());
    board.unmakeMove(m);
    return legal;
}
static void genPawnMoves(const Board& board, MoveList& list) {
    Color us=board.sideToMove(), them=~us;
    Bitboard pawns=board.pieces(us,PAWN), empty=board.empty(), enemies=board.pieces(them);
    Bitboard promoRank=(us==WHITE)?RankMask[RANK_8]:RankMask[RANK_1];
    Bitboard startRank=(us==WHITE)?RankMask[RANK_2]:RankMask[RANK_7];
    Bitboard push1=(us==WHITE?shiftN(pawns):shiftS(pawns))&empty;
    Bitboard push2=(us==WHITE?shiftN(shiftN(pawns&startRank)&empty):shiftS(shiftS(pawns&startRank)&empty))&empty;
    Bitboard captL=(us==WHITE?shiftNW(pawns):shiftSW(pawns))&enemies;
    Bitboard captR=(us==WHITE?shiftNE(pawns):shiftSE(pawns))&enemies;
    Bitboard p1np=push1&~promoRank, p1p=push1&promoRank;
    while(p1np){Square to=popLsb(p1np);list.add(encodeMove((us==WHITE)?to-8:to+8,to));}
    while(p1p ){Square to=popLsb(p1p );addPromotions(list,(us==WHITE)?to-8:to+8,to);}
    while(push2){Square to=popLsb(push2);list.add(encodeMove((us==WHITE)?to-16:to+16,to));}
    Bitboard clnp=captL&~promoRank,clp=captL&promoRank,crnp=captR&~promoRank,crp=captR&promoRank;
    while(clnp){Square to=popLsb(clnp);list.add(encodeMove((us==WHITE)?to-7:to+9,to));}
    while(crnp){Square to=popLsb(crnp);list.add(encodeMove((us==WHITE)?to-9:to+7,to));}
    while(clp ){Square to=popLsb(clp );addPromotions(list,(us==WHITE)?to-7:to+9,to);}
    while(crp ){Square to=popLsb(crp );addPromotions(list,(us==WHITE)?to-9:to+7,to);}
    Square ep=board.enPassant();
    if(ep!=NO_SQUARE){Bitboard epc=pawns&pawnAttacks(them,ep);while(epc){Square from=popLsb(epc);list.add(encodeMove(from,ep,EN_PASSANT));}}
}
static void genKnightMoves(const Board& board, MoveList& list){
    Color us=board.sideToMove(); Bitboard own=board.pieces(us),kts=board.pieces(us,KNIGHT);
    while(kts){Square f=popLsb(kts);addMoves(list,f,knightAttacks(f)&~own);}
}
static void genBishopMoves(const Board& board, MoveList& list){
    Color us=board.sideToMove(); Bitboard own=board.pieces(us),occ=board.occupied(),bbs=board.pieces(us,BISHOP);
    while(bbs){Square f=popLsb(bbs);addMoves(list,f,bishopAttacks(f,occ)&~own);}
}
static void genRookMoves(const Board& board, MoveList& list){
    Color us=board.sideToMove(); Bitboard own=board.pieces(us),occ=board.occupied(),rks=board.pieces(us,ROOK);
    while(rks){Square f=popLsb(rks);addMoves(list,f,rookAttacks(f,occ)&~own);}
}
static void genQueenMoves(const Board& board, MoveList& list){
    Color us=board.sideToMove(); Bitboard own=board.pieces(us),occ=board.occupied(),qs=board.pieces(us,QUEEN);
    while(qs){Square f=popLsb(qs);addMoves(list,f,queenAttacks(f,occ)&~own);}
}
static void genKingMoves(const Board& board, MoveList& list){
    Color us=board.sideToMove(),them=~us;
    Bitboard own=board.pieces(us),occ=board.occupied();
    Square from=board.kingSquare(us);
    addMoves(list,from,kingAttacks(from)&~own);
    if(us==WHITE){
        if(board.canCastle(WHITE_OO)&&board.pieceOn(H1)==WHITE_ROOK
            &&!(occ&(SquareBB[F1]|SquareBB[G1]))
            &&!isAttacked(board,E1,them)&&!isAttacked(board,F1,them)&&!isAttacked(board,G1,them))
            list.add(encodeMove(E1,G1,CASTLING));
        if(board.canCastle(WHITE_OOO)&&board.pieceOn(A1)==WHITE_ROOK
            &&!(occ&(SquareBB[B1]|SquareBB[C1]|SquareBB[D1]))
            &&!isAttacked(board,E1,them)&&!isAttacked(board,D1,them)&&!isAttacked(board,C1,them))
            list.add(encodeMove(E1,C1,CASTLING));
    } else {
        if(board.canCastle(BLACK_OO)&&board.pieceOn(H8)==BLACK_ROOK
            &&!(occ&(SquareBB[F8]|SquareBB[G8]))
            &&!isAttacked(board,E8,them)&&!isAttacked(board,F8,them)&&!isAttacked(board,G8,them))
            list.add(encodeMove(E8,G8,CASTLING));
        if(board.canCastle(BLACK_OOO)&&board.pieceOn(A8)==BLACK_ROOK
            &&!(occ&(SquareBB[B8]|SquareBB[C8]|SquareBB[D8]))
            &&!isAttacked(board,E8,them)&&!isAttacked(board,D8,them)&&!isAttacked(board,C8,them))
            list.add(encodeMove(E8,C8,CASTLING));
    }
}
void generateMoves(const Board& board, MoveList& list){
    Board& b=const_cast<Board&>(board);
    MoveList pseudo;
    genPawnMoves(board,pseudo); genKnightMoves(board,pseudo);
    genBishopMoves(board,pseudo); genRookMoves(board,pseudo);
    genQueenMoves(board,pseudo); genKingMoves(board,pseudo);
    for(int i=0;i<pseudo.count;++i)
        if(isLegal(b,pseudo.moves[i])) list.add(pseudo.moves[i]);
}
void generateCaptures(const Board& board, MoveList& list){
    MoveList all; generateMoves(board,all);
    for(int i=0;i<all.count;++i){
        Move m=all.moves[i];
        if(board.pieceOn(moveTo(m))!=NO_PIECE||moveType(m)==EN_PASSANT) list.add(m);
    }
}
