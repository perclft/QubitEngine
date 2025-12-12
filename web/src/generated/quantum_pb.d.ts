import * as jspb from 'google-protobuf'



export class CircuitRequest extends jspb.Message {
  getNumQubits(): number;
  setNumQubits(value: number): CircuitRequest;

  getOperationsList(): Array<GateOperation>;
  setOperationsList(value: Array<GateOperation>): CircuitRequest;
  clearOperationsList(): CircuitRequest;
  addOperations(value?: GateOperation, index?: number): GateOperation;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): CircuitRequest.AsObject;
  static toObject(includeInstance: boolean, msg: CircuitRequest): CircuitRequest.AsObject;
  static serializeBinaryToWriter(message: CircuitRequest, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): CircuitRequest;
  static deserializeBinaryFromReader(message: CircuitRequest, reader: jspb.BinaryReader): CircuitRequest;
}

export namespace CircuitRequest {
  export type AsObject = {
    numQubits: number,
    operationsList: Array<GateOperation.AsObject>,
  }
}

export class GateOperation extends jspb.Message {
  getType(): GateOperation.GateType;
  setType(value: GateOperation.GateType): GateOperation;

  getTargetQubit(): number;
  setTargetQubit(value: number): GateOperation;

  getControlQubit(): number;
  setControlQubit(value: number): GateOperation;

  getClassicalRegister(): number;
  setClassicalRegister(value: number): GateOperation;

  getAngle(): number;
  setAngle(value: number): GateOperation;

  getSecondControlQubit(): number;
  setSecondControlQubit(value: number): GateOperation;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): GateOperation.AsObject;
  static toObject(includeInstance: boolean, msg: GateOperation): GateOperation.AsObject;
  static serializeBinaryToWriter(message: GateOperation, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): GateOperation;
  static deserializeBinaryFromReader(message: GateOperation, reader: jspb.BinaryReader): GateOperation;
}

export namespace GateOperation {
  export type AsObject = {
    type: GateOperation.GateType,
    targetQubit: number,
    controlQubit: number,
    classicalRegister: number,
    angle: number,
    secondControlQubit: number,
  }

  export enum GateType { 
    HADAMARD = 0,
    PAULI_X = 1,
    CNOT = 2,
    MEASURE = 3,
    TOFFOLI = 4,
    PHASE_S = 5,
    PHASE_T = 6,
    ROTATION_Y = 7,
    ROTATION_Z = 8,
  }
}

export class StateResponse extends jspb.Message {
  getStateVectorList(): Array<StateResponse.ComplexNumber>;
  setStateVectorList(value: Array<StateResponse.ComplexNumber>): StateResponse;
  clearStateVectorList(): StateResponse;
  addStateVector(value?: StateResponse.ComplexNumber, index?: number): StateResponse.ComplexNumber;

  getClassicalResultsMap(): jspb.Map<number, boolean>;
  clearClassicalResultsMap(): StateResponse;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): StateResponse.AsObject;
  static toObject(includeInstance: boolean, msg: StateResponse): StateResponse.AsObject;
  static serializeBinaryToWriter(message: StateResponse, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): StateResponse;
  static deserializeBinaryFromReader(message: StateResponse, reader: jspb.BinaryReader): StateResponse;
}

export namespace StateResponse {
  export type AsObject = {
    stateVectorList: Array<StateResponse.ComplexNumber.AsObject>,
    classicalResultsMap: Array<[number, boolean]>,
  }

  export class ComplexNumber extends jspb.Message {
    getReal(): number;
    setReal(value: number): ComplexNumber;

    getImag(): number;
    setImag(value: number): ComplexNumber;

    serializeBinary(): Uint8Array;
    toObject(includeInstance?: boolean): ComplexNumber.AsObject;
    static toObject(includeInstance: boolean, msg: ComplexNumber): ComplexNumber.AsObject;
    static serializeBinaryToWriter(message: ComplexNumber, writer: jspb.BinaryWriter): void;
    static deserializeBinary(bytes: Uint8Array): ComplexNumber;
    static deserializeBinaryFromReader(message: ComplexNumber, reader: jspb.BinaryReader): ComplexNumber;
  }

  export namespace ComplexNumber {
    export type AsObject = {
      real: number,
      imag: number,
    }
  }

}

export class Measurement extends jspb.Message {
  getQubitIndex(): number;
  setQubitIndex(value: number): Measurement;

  getResult(): boolean;
  setResult(value: boolean): Measurement;

  getProbability(): number;
  setProbability(value: number): Measurement;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): Measurement.AsObject;
  static toObject(includeInstance: boolean, msg: Measurement): Measurement.AsObject;
  static serializeBinaryToWriter(message: Measurement, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): Measurement;
  static deserializeBinaryFromReader(message: Measurement, reader: jspb.BinaryReader): Measurement;
}

export namespace Measurement {
  export type AsObject = {
    qubitIndex: number,
    result: boolean,
    probability: number,
  }
}

