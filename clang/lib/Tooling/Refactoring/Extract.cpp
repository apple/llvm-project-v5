//===--- Extract.cpp - Clang refactoring library --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Implements the "extract" refactoring that can pull code into
/// new functions, methods or declare new variables.
///
//===----------------------------------------------------------------------===//

#include "clang/Tooling/Refactoring/Extract/Extract.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/Rewrite/Core/Rewriter.h"

namespace clang {
namespace tooling {

namespace {

/// Returns true if \c E is a simple literal or a reference expression that
/// should not be extracted.
bool isSimpleExpression(const Expr *E) {
  if (!E)
    return false;
  switch (E->IgnoreParenCasts()->getStmtClass()) {
  case Stmt::DeclRefExprClass:
  case Stmt::PredefinedExprClass:
  case Stmt::IntegerLiteralClass:
  case Stmt::FloatingLiteralClass:
  case Stmt::ImaginaryLiteralClass:
  case Stmt::CharacterLiteralClass:
  case Stmt::StringLiteralClass:
    return true;
  default:
    return false;
  }
}

SourceLocation computeFunctionExtractionLocation(const Decl *D) {
  // FIXME (Alex L): Method -> function extraction should place function before
  // C++ record if the method is defined inside the record.
  return D->getLocStart();
}

} // end anonymous namespace

const RefactoringDescriptor &ExtractFunction::describe() {
  static const RefactoringDescriptor Descriptor = {
      "extract-function",
      "Extract Function",
      "(WIP action; use with caution!) Extracts code into a new function",
  };
  return Descriptor;
}

Expected<ExtractFunction>
ExtractFunction::initiate(RefactoringRuleContext &Context,
                          CodeRangeASTSelection Code,
                          Optional<std::string> DeclName) {
  // We would like to extract code out of functions/methods/blocks.
  // Prohibit extraction from things like global variable / field
  // initializers and other top-level expressions.
  if (!Code.isInFunctionLikeBodyOfCode())
    return Context.createDiagnosticError(
        diag::err_refactor_code_outside_of_function);

  // Avoid extraction of simple literals and references.
  if (Code.size() == 1 && isSimpleExpression(dyn_cast<Expr>(Code[0])))
    return Context.createDiagnosticError(
        diag::err_refactor_extract_simple_expression);

  // FIXME (Alex L): Prohibit extraction of Objective-C property setters.
  return ExtractFunction(std::move(Code), DeclName);
}

// FIXME: Support C++ method extraction.
// FIXME: Support Objective-C method extraction.
Expected<AtomicChanges>
ExtractFunction::createSourceReplacements(RefactoringRuleContext &Context) {
  const Decl *ParentDecl = Code.getFunctionLikeNearestParent();
  assert(ParentDecl && "missing parent");

  // Compute the source range of the code that should be extracted.
  SourceRange ExtractedRange(Code[0]->getLocStart(),
                             Code[Code.size() - 1]->getLocEnd());
  // FIXME (Alex L): Add code that accounts for macro locations.

  ASTContext &AST = Context.getASTContext();
  SourceManager &SM = AST.getSourceManager();
  const LangOptions &LangOpts = AST.getLangOpts();
  Rewriter ExtractedCodeRewriter(SM, LangOpts);

  // FIXME: Capture used variables.

  // Compute the return type.
  QualType ReturnType = AST.VoidTy;
  // FIXME (Alex L): Account for the return statement in extracted code.
  // FIXME (Alex L): Check for lexical expression instead.
  bool IsExpr = Code.size() == 1 && isa<Expr>(Code[0]);
  if (IsExpr) {
    // FIXME (Alex L): Get a more user-friendly type if needed.
    ReturnType = cast<Expr>(Code[0])->getType();
  }

  // FIXME: Rewrite the extracted code performing any required adjustments.

  // FIXME: Capture any field if necessary (method -> function extraction).

  // FIXME: Sort captured variables by name.

  // FIXME: Capture 'this' / 'self' if necessary.

  // FIXME: Compute the actual parameter types.

  // Compute the location of the extracted declaration.
  SourceLocation ExtractedDeclLocation =
      computeFunctionExtractionLocation(ParentDecl);
  // FIXME: Adjust the location to account for any preceding comments.

  // FIXME: Adjust with PP awareness like in Sema to get correct 'bool'
  // treatment.
  PrintingPolicy PP = AST.getPrintingPolicy();
  // FIXME: PP.UseStdFunctionForLambda = true;
  PP.SuppressStrongLifetime = true;
  PP.SuppressLifetimeQualifiers = true;
  PP.SuppressUnwrittenScope = true;

  AtomicChange Change(SM, ExtractedDeclLocation);
  // Create the replacement for the extracted declaration.
  {
    std::string ExtractedCode;
    llvm::raw_string_ostream OS(ExtractedCode);
    // FIXME: Use 'inline' in header.
    OS << "static ";
    ReturnType.print(OS, PP, DeclName);
    OS << '(';
    // FIXME: Arguments.
    OS << ')';

    // Function body.
    OS << " {\n";
    if (IsExpr && !ReturnType->isVoidType())
      OS << "return ";
    OS << ExtractedCodeRewriter.getRewrittenText(ExtractedRange);
    // FIXME: Compute the correct semicolon policy.
    OS << ';';
    OS << "\n}\n\n";
    auto Err = Change.insert(SM, ExtractedDeclLocation, OS.str());
    if (Err)
      return std::move(Err);
  }

  // Create the replacement for the call to the extracted declaration.
  {
    std::string ReplacedCode;
    llvm::raw_string_ostream OS(ReplacedCode);

    OS << DeclName << '(';
    // FIXME: Forward arguments.
    OS << ')';
    // FIXME: Add semicolon if needed.

    auto Err = Change.replace(
        SM, CharSourceRange::getTokenRange(ExtractedRange), OS.str());
    if (Err)
      return std::move(Err);
  }

  // FIXME: Add support for assocciated symbol location to AtomicChange to mark
  // the ranges of the name of the extracted declaration.
  return AtomicChanges{std::move(Change)};
}

} // end namespace tooling
} // end namespace clang
