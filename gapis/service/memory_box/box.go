// Copyright (C) 2019 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package memory_box

import (
	"context"
	"fmt"

	"github.com/google/gapid/core/data/pod"
	"github.com/google/gapid/core/log"
	"github.com/google/gapid/core/os/device"
	"github.com/google/gapid/gapis/memory"
	"github.com/google/gapid/gapis/service/types"
)

func Box(ctx context.Context, d *memory.Decoder, t *types.Type) (*Value, error) {
	a, err := t.Alignment(ctx, d.MemoryLayout())
	if err != nil {
		return nil, err
	}
	d.Align(uint64(a))
	switch t := t.Ty.(type) {
	case *types.Type_Pod:
		switch t.Pod {
		case pod.Type_uint:
			v := d.Uint()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Uint{
							Uint: uint64(v),
						},
					},
				}}, d.Error()
		case pod.Type_sint:
			v := d.Int()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Sint{
							Sint: int64(v),
						},
					},
				}}, d.Error()
		case pod.Type_uint8:
			v := d.U8()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Uint8{
							Uint8: uint32(v),
						},
					},
				}}, d.Error()
		case pod.Type_sint8:
			v := d.I8()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Sint8{
							Sint8: int32(v),
						},
					},
				}}, d.Error()
		case pod.Type_uint16:
			v := d.U16()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Uint16{
							Uint16: uint32(v),
						},
					},
				}}, d.Error()
		case pod.Type_sint16:
			v := d.I16()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Sint16{
							Sint16: int32(v),
						},
					},
				}}, d.Error()
		case pod.Type_uint32:
			v := d.U32()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Uint32{
							Uint32: v,
						},
					},
				}}, d.Error()
		case pod.Type_sint32:
			v := d.I32()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Sint32{
							Sint32: v,
						},
					},
				}}, d.Error()
		case pod.Type_uint64:
			v := d.U64()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Uint64{
							Uint64: v,
						},
					},
				}}, d.Error()
		case pod.Type_sint64:
			v := d.I64()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Sint64{
							Sint64: v,
						},
					},
				}}, d.Error()
		case pod.Type_bool:
			v := d.Bool()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Bool{
							Bool: v,
						},
					},
				}}, d.Error()
		case pod.Type_string:
			v := d.String()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_String_{
							String_: v,
						},
					},
				}}, d.Error()
		case pod.Type_float32:
			v := d.F32()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Float32{
							Float32: v,
						},
					},
				}}, d.Error()
		case pod.Type_float64:
			v := d.F64()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Float64{
							Float64: v,
						},
					},
				}}, d.Error()
		}
	case *types.Type_Pointer:
		v := d.Pointer()
		return &Value{
			Val: &Value_Pointer{
				Pointer: &Pointer{
					Address: v,
				},
			}}, d.Error()
	case *types.Type_Struct:
		s := &Struct{
			Fields: []*Value{},
		}

		for _, f := range t.Struct.Fields {
			elem, ok := types.TryGetType(f.Type)
			if !ok {
				return nil, log.Err(ctx, nil, "Incomplete type in struct box")
			}

			v, err := Box(ctx, d, elem)
			if err != nil {
				return nil, err
			}
			s.Fields = append(s.Fields, v)
		}
		d.Align(uint64(a))
		return &Value{
			Val: &Value_Struct{
				Struct: s,
			}}, nil
	case *types.Type_Sized:
		switch t.Sized {
		case types.SizedType_sized_int:
			v := d.Int()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Sint64{
							Sint64: int64(v),
						},
					},
				}}, d.Error()
		case types.SizedType_sized_uint:
			v := d.Uint()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Uint64{
							Uint64: uint64(v),
						},
					},
				}}, d.Error()
		case types.SizedType_sized_size:
			v := d.Size()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Uint64{
							Uint64: uint64(v),
						},
					},
				}}, d.Error()
		case types.SizedType_sized_char:
			v := d.Char()
			return &Value{
				Val: &Value_Pod{
					Pod: &pod.Value{
						Val: &pod.Value_Uint8{
							Uint8: uint32(v),
						},
					},
				}}, d.Error()
		}
	case *types.Type_Pseudonym:
		if elem, ok := types.TryGetType(t.Pseudonym.Underlying); ok {
			return Box(ctx, d, elem)
		}
	case *types.Type_Array:
		if elem, ok := types.TryGetType(t.Array.ElementType); ok {
			s := &Array{
				Entries: []*Value{},
			}
			for i := uint64(0); i < t.Array.Size; i++ {
				v, err := Box(ctx, d, elem)
				if err != nil {
					return nil, err
				}
				s.Entries = append(s.Entries, v)
			}
			return &Value{
				Val: &Value_Array{
					Array: s,
				},
			}, nil
		}
	case *types.Type_Enum:
		if elem, ok := types.TryGetType(t.Enum.Underlying); ok {
			return Box(ctx, d, elem)
		}
	case *types.Type_Map:
		return nil, log.Err(ctx, nil, "Cannot decode map from memory")
	case *types.Type_Reference:
		return nil, log.Err(ctx, nil, "Cannot decode refs from memory")
	case *types.Type_Slice:
		return nil, log.Err(ctx, nil, "Cannot decode slices from memory")
	}
	return nil, log.Err(ctx, nil, "Unhandled box type")
}

func DecodeMemory(ctx context.Context, l *device.MemoryLayout, dec *memory.Decoder, size uint64, e *types.Type)  (*Value, error) {
	sz := size
	nElems := 1
	elemSize := 0
	isSlice := false
	ty := e
	var err error
	if sl, ok := e.Ty.(*types.Type_Slice); ok {
		isSlice = true
		sliceType, err := types.GetType(sl.Slice.Underlying)
		if err != nil {
			return nil, err
		}
		elemSize, err = sliceType.Size(ctx, l)
		if err != nil {
			return nil, err
		}
		if size == 0 {
			return nil, log.Err(ctx, nil, "Cannot have an unsized range with a slice")
		}
		nElems = int(sz / uint64(elemSize))
		ty = sliceType
	} else {
		elemSize, err = e.Size(ctx, l)
		if err != nil {
			return nil, err
		}
	}
	vals := []*Value{}
	for i := 0; i < nElems; i++ {
		v, err := Box(ctx, dec, ty)
		if err != nil {
			return nil, err
		}
		vals = append(vals, v)
	}

	if isSlice {
		return &Value{
			Val: &Value_Slice{
				Slice: &Slice{
					Values: vals,
				}}}, nil
	}

	return vals[0], nil
}

func EncodeMemory(ctx context.Context, ptrResolver func(uint64)uint64, l *device.MemoryLayout, e *memory.Encoder, t *types.Type, v* Value)  error {
	if st, ok := t.Ty.(*types.Type_Slice); ok {
		underlying, ok := types.TryGetType(st.Slice.Underlying)
		if !ok {
			return fmt.Errorf("Invalid child type of %v", st)
		}
		sl := v.Val.(*Value_Slice).Slice
		for i := range(sl.Values) {
			if err := Unbox(ctx, ptrResolver, e, underlying, sl.Values[i]); err != nil {
				return err
			}
		}
	}
	return fmt.Errorf("It is only valid to encode slice types")
}

func Unbox(ctx context.Context, ptrResolver func(uint64)uint64, e *memory.Encoder, t *types.Type, v *Value) error {
	a, err := t.Alignment(ctx, e.MemoryLayout())
	if err != nil {
		return err
	}
	e.Align(uint64(a))

	switch t := t.Ty.(type) {
	case *types.Type_Pod:
		pv := v.Val.(*Value_Pod).Pod.Val
		switch t.Pod {
		case pod.Type_uint:
			e.Uint(memory.Uint(pv.(*pod.Value_Uint).Uint))
			return nil
		case pod.Type_sint:
			e.Int(memory.Int(pv.(*pod.Value_Sint).Sint))
			return nil
		case pod.Type_uint8:
			e.U8(uint8(pv.(*pod.Value_Uint8).Uint8))
			return nil
		case pod.Type_sint8:
			e.I8(int8(pv.(*pod.Value_Sint8).Sint8))
			return nil
		case pod.Type_uint16:
			e.U16(uint16(pv.(*pod.Value_Uint16).Uint16))
			return nil
		case pod.Type_sint16:
			e.I16(int16(pv.(*pod.Value_Sint16).Sint16))
			return nil
		case pod.Type_uint32:
			e.U32(uint32(pv.(*pod.Value_Uint32).Uint32))
			return nil
		case pod.Type_sint32:
			e.I32(int32(pv.(*pod.Value_Sint32).Sint32))
			return nil
		case pod.Type_uint64:
			e.U64(uint64(pv.(*pod.Value_Uint64).Uint64))
			return nil
		case pod.Type_sint64:
			e.I64(int64(pv.(*pod.Value_Sint64).Sint64))
			return nil
		case pod.Type_bool:
			e.Bool(pv.(*pod.Value_Bool).Bool)
			return nil
		case pod.Type_string:
			e.String(pv.(*pod.Value_String_).String_)
			return nil
		case pod.Type_float32:
			e.F32(pv.(*pod.Value_Float32).Float32)
			return nil
		case pod.Type_float64:
			e.F64(pv.(*pod.Value_Float64).Float64)
			return nil
		}
	case *types.Type_Pointer:
		ptr := v.Val.(*Value_Pointer).Pointer
		v := ptr.Address
		if ptr.Fictional {
			v = ptrResolver(v)
		}
		e.Pointer(v)
		return nil
	case *types.Type_Struct:
		s := v.Val.(*Value_Struct).Struct
		if len(s.Fields) != len(t.Struct.Fields) {
			return log.Err(ctx, nil, "Invalid struct detected");
		}
		for i, f := range t.Struct.Fields {
			elem, ok := types.TryGetType(f.Type)
			if !ok {
				return log.Err(ctx, nil, "Incomplete type in struct unbox")
			}
			err := Unbox(ctx, ptrResolver, e, elem, s.Fields[i])
			if err != nil {
				return err
			}
		}
		e.Align(uint64(a))
	case *types.Type_Sized:
		switch t.Sized {
		case types.SizedType_sized_int:
			e.Int(memory.Int(v.Val.(*Value_Pod).Pod.Val.(*pod.Value_Sint64).Sint64))
			return nil
		case types.SizedType_sized_uint:
			e.Uint(memory.Uint(v.Val.(*Value_Pod).Pod.Val.(*pod.Value_Uint64).Uint64))
		case types.SizedType_sized_size:
			e.Size(memory.Size(v.Val.(*Value_Pod).Pod.Val.(*pod.Value_Uint64).Uint64))
		case types.SizedType_sized_char:
			e.Char(memory.Char(v.Val.(*Value_Pod).Pod.Val.(*pod.Value_Uint8).Uint8))
		}
	case *types.Type_Pseudonym:
		if elem, ok := types.TryGetType(t.Pseudonym.Underlying); ok {
			return Unbox(ctx, ptrResolver, e, elem, v)
		} else {
			return log.Err(ctx, nil, "Incomplete pseudonym type in unbox")
		}
	case *types.Type_Array:
		if elem, ok := types.TryGetType(t.Array.ElementType); ok {
			arr := v.Val.(*Value_Array).Array
			if len(arr.Entries) != int(t.Array.Size) {
				return log.Err(ctx, nil, "Invalid array serialization")
			}
			for i := uint64(0); i < t.Array.Size; i++ {
				if err := Unbox(ctx, ptrResolver, e, elem, arr.Entries[i]); err != nil {
					return err
				}
			}
		} else {
			return log.Err(ctx, nil, "Incomplete pseudonym type in unbox")
		}
	case *types.Type_Enum:
		if elem, ok := types.TryGetType(t.Enum.Underlying); ok {
			return Unbox(ctx, ptrResolver, e, elem, v)
		} else {
			return log.Err(ctx, nil, "Incomplete enum type in unbox")
		}
	case *types.Type_Map:
		return log.Err(ctx, nil, "Cannot encode map to memory")
	case *types.Type_Reference:
		return log.Err(ctx, nil, "Cannot encode refs to memory")
	case *types.Type_Slice:
		return log.Err(ctx, nil, "Cannot encode slices to memory")
	}
	return log.Err(ctx, nil, "Unhandled box type")
}
