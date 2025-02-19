#include "core/lsp/QueryResponse.h"
#include "main/lsp/lsp.h"

using namespace std;

namespace sorbet::realmain::lsp {

string methodSignatureString(const core::GlobalState &gs, const core::TypePtr &retType,
                             const core::DispatchResult &dispatchResult,
                             const unique_ptr<core::TypeConstraint> &constraint) {
    string contents = "";
    auto start = &dispatchResult;
    ;
    while (start != nullptr) {
        auto &dispatchComponent = start->main;
        if (dispatchComponent.method.exists()) {
            if (!contents.empty()) {
                contents += " ";
            }
            contents += methodDetail(gs, dispatchComponent.method, dispatchComponent.receiver, retType, constraint);
        }
        start = start->secondary.get();
    }
    return contents;
}

unique_ptr<MarkupContent> formatRubyCode(MarkupKind markupKind, string str) {
    if (markupKind == MarkupKind::Markdown && str.length() > 0) {
        str = fmt::format("```ruby\n{}\n```", str);
    }
    return make_unique<MarkupContent>(markupKind, move(str));
}

LSPResult LSPLoop::handleTextDocumentHover(unique_ptr<core::GlobalState> gs, const MessageId &id,
                                           const TextDocumentPositionParams &params) const {
    auto response = make_unique<ResponseMessage>("2.0", id, LSPMethod::TextDocumentHover);
    prodCategoryCounterInc("lsp.messages.processed", "textDocument.hover");

    auto result =
        setupLSPQueryByLoc(move(gs), params.textDocument->uri, *params.position, LSPMethod::TextDocumentHover);
    gs = move(result.gs);
    if (result.error) {
        // An error happened while setting up the query.
        response->error = move(result.error);
    } else {
        auto &queryResponses = result.responses;
        if (queryResponses.empty()) {
            // Note: Need to specifically specify the variant type here so the null gets placed into the proper slot.
            response->result = variant<JSONNullObject, unique_ptr<Hover>>(JSONNullObject());
            return LSPResult::make(move(gs), move(response));
        }

        auto resp = move(queryResponses[0]);
        if (auto sendResp = resp->isSend()) {
            auto retType = sendResp->dispatchResult->returnType;
            auto &constraint = sendResp->dispatchResult->main.constr;
            if (constraint) {
                retType = core::Types::instantiate(core::Context(*gs, core::Symbols::root()), retType, *constraint);
            }
            response->result = make_unique<Hover>(formatRubyCode(
                clientHoverMarkupKind, methodSignatureString(*gs, retType, *sendResp->dispatchResult, constraint)));
        } else if (auto defResp = resp->isDefinition()) {
            response->result = make_unique<Hover>(formatRubyCode(
                clientHoverMarkupKind, methodDetail(*gs, defResp->symbol, nullptr, defResp->retType.type, nullptr)));
        } else if (auto constResp = resp->isConstant()) {
            const auto &data = constResp->symbol.data(*gs);
            auto type = constResp->retType.type;
            if (data->isClass()) {
                auto singletonClass = data->lookupSingletonClass(*gs);
                ENFORCE(singletonClass.exists(), "Every class should have a singleton class by now.");
                type = singletonClass.data(*gs)->externalType(*gs);
            } else if (data->isStaticField() && data->isTypeAlias()) {
                // By wrapping the type in `MetaType`, we display a type alias of `Foo` as `<Type: Foo>` rather than
                // `Foo`.
                type = core::make_type<core::MetaType>(type);
            }
            response->result = make_unique<Hover>(formatRubyCode(clientHoverMarkupKind, type->showWithMoreInfo(*gs)));
        } else {
            response->result =
                make_unique<Hover>(formatRubyCode(clientHoverMarkupKind, resp->getRetType()->showWithMoreInfo(*gs)));
        }
    }
    return LSPResult::make(move(gs), move(response));
}
} // namespace sorbet::realmain::lsp
