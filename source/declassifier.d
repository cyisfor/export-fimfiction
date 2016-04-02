enum Event {
  Text,
  SelfClosing,
  OpenStart,
  OpenEnd,
  Close,
  AttrName,
  AttrEnd,
  AttrValue,
  Comment,
  Declaration,
  ProcessingInstruction,
  CDATA,
  NamedEntity,
  NumericEntity,
  HexEntity,
  DocumentEnd
}

enum string[Event] names = [
  Event.Text: "Text",
  Event.SelfClosing: "SelfClosing",
  Event.OpenStart: "OpenStart",
  Event.OpenEnd: "OpenEnd",
  Event.Close: "Close",
  Event.AttrName: "AttrName",
  Event.AttrEnd: "AttrEnd",
  Event.AttrValue: "AttrValue",
  Event.Comment: "Comment",
  Event.Declaration: "Declaration",
  Event.ProcessingInstruction: "ProcessingInstruction",
  Event.CDATA: "CDATA",
  Event.NamedEntity: "NamedEntity",
  Event.NumericEntity: "NumericEntity",
  Event.HexEntity: "HexEntity",
  Event.DocumentEnd: "DocumentEnd"
];

interface Handler {
  void onEntity(const(char)[] data, const(char)[] decoded); // ugh...
  void handle(Event event, const(char)[] data);
  void handle(Event event);
}

struct Declassifier {
  Handler h;
  void onText(const(char)[] arg0){ this.h.handle(Event.Text, arg0); }
  void onSelfClosing(){ this.h.handle(Event.SelfClosing); }
  void onOpenStart(const(char)[] arg0){ this.h.handle(Event.OpenStart, arg0); }
  void onOpenEnd(const(char)[] arg0){ this.h.handle(Event.OpenEnd, arg0); }
  void onClose(const(char)[] arg0){ this.h.handle(Event.Close, arg0); }
  void onAttrName(const(char)[] arg0){ this.h.handle(Event.AttrName, arg0); }
  void onAttrEnd(){ this.h.handle(Event.AttrEnd); }
  void onAttrValue(const(char)[] arg0){ this.h.handle(Event.AttrValue, arg0); }
  void onComment(const(char)[] arg0){ this.h.handle(Event.Comment, arg0); }
  void onDeclaration(const(char)[] arg0){ this.h.handle(Event.Declaration, arg0); }
  void onProcessingInstruction(const(char)[] arg0){ this.h.handle(Event.ProcessingInstruction, arg0); }
  void onCDATA(const(char)[] arg0){ this.h.handle(Event.CDATA, arg0); }
  void onNamedEntity(const(char)[] arg0){ this.h.handle(Event.NamedEntity, arg0); }
  void onNumericEntity(const(char)[] arg0){ this.h.handle(Event.NumericEntity, arg0); }
  void onHexEntity(const(char)[] arg0){ this.h.handle(Event.HexEntity, arg0); }
  void onDocumentEnd(){ this.h.handle(Event.DocumentEnd); }

  void onEntity(const(char)[] data, const(char)[] decoded){ this.h.onEntity(data,decoded); }
}
