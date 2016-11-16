package com.ardash.hashcash.test;

/*
 Copyright 2016 Andreas Redmer <a_r@posteo.de>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 */

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.util.Arrays;
import java.util.Collection;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

import com.ardash.hashcash.HashCash;

@RunWith(Parameterized.class)
public class DeterministicValidationTest {
    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {
        { "1:48:110416:etienne@cri.fr:::000A2F00000063BF012", 48, true },
        { "1:44:070217:foo::xSi0bPjoswUh6h1Y:TMNI7", 44, true },
        { "1:42:060922:When I think of all the good times that I've wasted ...::UXkz/DsCCgfvBVtH:00000EF7+j", 42, true },
        { "1:41:060704:president@whitehouse.gov::XxcHzSfxDZ38cwRu:000000000000000000000000000000000000m2EDd", 41, true },
        { "1:40:051222:foo@bar.org::Cu2iqc4SmotZ7MRR:0000214c3J", 40, true },
        { "1:20:040806:foo::65f460d0726f420d:13a6b8", 20, true },

        // TODO add Adam Back's test
        
        { "1:1:111111::::", 0, false },
        { "1:0:111111::::", 0, true }
           });
    }

    private String inputStamp;
    private int bitsExpected;
    private boolean validExpected;

    public DeterministicValidationTest(String inputStamp, int bitsExpected, boolean validExpected) {
    	this.inputStamp= inputStamp;
        this.bitsExpected= bitsExpected;
        this.validExpected= validExpected;
    }

    @Test
    public void test() {
    	HashCash hc = HashCash.parseFromString(inputStamp);
        assertTrue(hc.isValid()==validExpected);
        assertEquals(hc.getValue(), bitsExpected);
    }
}